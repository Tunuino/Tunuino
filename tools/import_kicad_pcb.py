#!/usr/bin/env python3
# Copyright (c) 2026 Tunuino.
"""Extract assigned pad nets from a legacy KiCad PCB; no circuit inference."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from kicad_sexpr import MAX_BYTES, parse, children, single
from import_kicad_netlist import text


def import_pcb(data, *, family, repository, commit, source_path, connectors, targets):
    if family not in ('proteus','uaefi'):
        raise ValueError('Unknown family')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9_.-]+',repository):
        raise ValueError('Invalid repository')
    if not re.fullmatch(r'[0-9a-f]{40}',commit):
        raise ValueError('An exact Git commit SHA is required')
    path = PurePosixPath(text(source_path,'source path'))
    if path.is_absolute() or '..' in path.parts or '\\' in source_path or ':' in source_path:
        raise ValueError('Source path must be repository relative')
    if not connectors or len(set(connectors)) != len(connectors) or len(set(targets)) != len(targets) or set(connectors) & set(targets):
        raise ValueError('Select unique, distinct connectors and targets')
    root = parse(data)
    if root[0] != 'kicad_pcb' or single(root,'version') != ['version','20171130']:
        raise ValueError('Only legacy PCB format 20171130 is supported')
    title = single(root,'title_block',False)
    rev = single(title,'rev',False) if title else None
    if rev is not None and len(rev) != 2:
        raise ValueError('Malformed PCB revision')
    revision = text(rev[1],'PCB revision',64) if rev else None
    nets, names = {}, set()
    for net in children(root,'net'):
        if len(net) != 3 or not isinstance(net[1],str) or not isinstance(net[2],str) or not net[1].isascii() or not net[1].isdecimal():
            raise ValueError('Invalid net declaration')
        code = int(net[1]); name = net[2]
        if code in nets or name in names or (code == 0 and name != '') or (code != 0 and not name):
            raise ValueError('Duplicate or invalid net declaration')
        if code:
            text(name,'net name')
        nets[code] = name; names.add(name)
    components, nodes = {}, {}
    modules = children(root,'module')
    if len(modules) > 20000 or len(nets) > 50000:
        raise ValueError('PCB inventory exceeds limits')
    selected = set(connectors) | set(targets)
    selected_modules = {}
    anonymous_footprints = 0
    for index, module in enumerate(modules):
        fields = children(module,'fp_text')
        refs = [field for field in fields if len(field) >= 3 and field[1] == 'reference']
        if len(refs) != 1:
            raise ValueError('Footprint has no unique reference')
        ref = refs[0][2]
        if not isinstance(ref,str):
            raise ValueError('Invalid footprint reference')
        pads = children(module,'pad')
        # Decorative footprints may share empty/placeholder references. They
        # cannot become electrical nodes or silently satisfy selected roles.
        electrical = any(len(pad) > 1 and pad[1] != '' for pad in pads)
        if ref in selected:
            if ref in selected_modules:
                raise ValueError('Selected footprint reference is ambiguous')
            selected_modules[ref] = module
        if not electrical:
            continue
        if ref:
            text(ref,'electrical footprint reference',64)
            if ref in components:
                raise ValueError('Duplicate electrical footprint reference')
            components[ref] = module
        else:
            anonymous_footprints += 1
        for pad in pads:
            if len(pad) < 4:
                raise ValueError('Invalid pad')
            assigned = single(pad,'net',False)
            if pad[1] == '':
                if assigned and (len(assigned) != 3 or assigned[1:] != ['0','']):
                    raise ValueError('Assigned pad has no number')
                continue
            pin = text(pad[1],'pad number',128)
            code = 0
            if assigned is not None:
                if len(assigned) != 3 or not isinstance(assigned[1],str) or not isinstance(assigned[2],str) or not assigned[1].isascii() or not assigned[1].isdecimal():
                    raise ValueError('Invalid pad net')
                code = int(assigned[1])
                if code not in nets or nets[code] != assigned[2]:
                    raise ValueError('Unknown or mismatched pad net')
            key = (ref,index if not ref else None,pin)
            if key in nodes:
                if nodes[key]['code'] != code:
                    raise ValueError('Repeated pad number has conflicting nets')
                nodes[key]['pad_instances'] += 1
            else:
                nodes[key] = {'ref':ref or None,'pin':pin,'code':code,'pad_instances':1}
            if len(nodes) > 200000:
                raise ValueError('Too many pads')
    for ref in selected:
        if ref not in selected_modules or ref not in components:
            raise ValueError(f'Unknown or non-electrical selected reference {ref}')
    pins, selected_codes = [], set()
    for (ref,index,pin), node in nodes.items():
        if ref in connectors:
            code = node['code']
            pins.append({'connector':ref,'pin':pin,'net':nets[code] if code else None,
                'pad_instances':node['pad_instances']})
            if code:
                selected_codes.add(code)
    net_records = []
    for code in sorted(selected_codes,key=lambda c:nets[c]):
        members = [{k:v for k,v in node.items() if k != 'code'} for node in nodes.values() if node['code'] == code and node['ref'] is not None]
        anonymous = sum(node['code'] == code and node['ref'] is None for node in nodes.values())
        members.sort(key=lambda n:(n['ref'],n['pin']))
        net_records.append({'name':nets[code],'named_nodes':members,'anonymous_endpoint_count':anonymous,
            'selected_target_nodes':[n for n in members if n['ref'] in targets]})
    selected_components = []
    for ref in sorted(selected):
        module = selected_modules[ref]
        values = [field[2] for field in children(module,'fp_text') if len(field) >= 3 and field[1] == 'value']
        if len(values) != 1:
            raise ValueError('Selected footprint has no unique value')
        selected_components.append({'ref':ref,'role':'connector' if ref in connectors else 'target',
            'value':text(values[0],'component value'),'footprint':text(module[1],'footprint')})
    return {'format':'tunuino-pcb-membership-v1','family':family,'revision':revision,
        'revision_status':'declared_in_pcb' if revision else 'not_declared_in_pcb',
        'validation':'unreviewed','firmware_pin_definitions_generated':False,
        'scope':'PCB pad net assignments only; no copper continuity or schematic equivalence check',
        'source':{'repository':repository,'commit':commit,'path':source_path,'file_format_version':'20171130',
            'sha256':hashlib.sha256(data).hexdigest(),
            'git_blob_sha1':hashlib.sha1(b'blob '+str(len(data)).encode('ascii')+b'\0'+data).hexdigest()},
        'source_counts':{'footprints':len(modules),'electrical_footprints':len(components),
            'anonymous_electrical_footprints':anonymous_footprints,'assigned_nets':sum(code != 0 for code in nets),'numbered_endpoints':len(nodes)},
        'selected_components':selected_components,'nets':net_records,'pins':sorted(pins,key=lambda p:(p['connector'],p['pin']))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('pcb',type=Path)
    for name in ('family','repository','commit','source-path'):
        parser.add_argument('--'+name,required=True)
    parser.add_argument('--connector',action='append',required=True)
    parser.add_argument('--target',action='append',default=[])
    parser.add_argument('--output',type=Path,required=True)
    args = parser.parse_args()
    try:
        with args.pcb.open('rb') as source:
            data = source.read(MAX_BYTES+1)
        report = import_pcb(data,family=args.family,repository=args.repository,commit=args.commit,
            source_path=args.source_path,connectors=args.connector,targets=args.target)
        encoded = json.dumps(report,ensure_ascii=False,indent=2)+'\n'
        with args.output.open('x',encoding='utf-8',newline='\n') as destination:
            destination.write(encoded)
    except (OSError,ValueError,IndexError,TypeError) as error:
        parser.exit(1,f'Import failed: {error}\n')


if __name__ == '__main__':
    main()
