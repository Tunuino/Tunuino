#!/usr/bin/env python3
# Copyright (c) 2026 Tunuino.
"""Compare selected PCB components with one directly referenced schematic sheet."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from kicad_sexpr import MAX_BYTES, parse, children, single
from import_kicad_netlist import text


def properties(node):
    result = {}
    for prop in children(node,'property'):
        if len(prop) < 3 or not isinstance(prop[1],str) or not isinstance(prop[2],str) or prop[1] in result:
            raise ValueError('Malformed or duplicate symbol property')
        result[prop[1]] = prop[2]
    return result


def scalar(node,key,required=True):
    child = single(node,key,required)
    if child is None:
        return None
    if len(child) != 2:
        raise ValueError(f'Malformed {key}')
    return text(child[1],key)


def read_schematic(data):
    root = parse(data)
    if root[0] != 'kicad_sch' or single(root,'version') != ['version','20230121']:
        raise ValueError('Only schematic format 20230121 is supported')
    return root


def source_record(data,path):
    path = text(path,'source path')
    if PurePosixPath(path).is_absolute() or '..' in PurePosixPath(path).parts or '\\' in path or ':' in path:
        raise ValueError('Source paths must be repository relative')
    return {'path':path,'sha256':hashlib.sha256(data).hexdigest(),
        'git_blob_sha1':hashlib.sha1(b'blob '+str(len(data)).encode('ascii')+b'\0'+data).hexdigest()}


def compare(pcb_report,root_data,sheet_data,*,root_path,sheet_path):
    if pcb_report.get('format') != 'tunuino-pcb-membership-v1':
        raise ValueError('Expected PCB membership report')
    source = pcb_report.get('source',{})
    if not re.fullmatch(r'[0-9a-f]{40}',source.get('commit','')) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9_.-]+',source.get('repository','')):
        raise ValueError('PCB report has invalid repository/commit')
    root_source, sheet_source = source_record(root_data,root_path), source_record(sheet_data,sheet_path)
    root, leaf = read_schematic(root_data), read_schematic(sheet_data)
    root_id = scalar(root,'uuid')
    matches = [sheet for sheet in children(root,'sheet')
        if properties(sheet).get('Sheetfile') == str(PurePosixPath(sheet_path).relative_to(PurePosixPath(root_path).parent))]
    if len(matches) != 1:
        raise ValueError('Selected leaf must be directly referenced exactly once')
    instance_path = '/' + root_id + '/' + scalar(matches[0],'uuid')
    project = PurePosixPath(root_path).stem
    root_title, leaf_title = single(root,'title_block',False), single(leaf,'title_block',False)
    root_rev = scalar(root_title,'rev',False) if root_title else None
    leaf_rev = scalar(leaf_title,'rev',False) if leaf_title else None
    selected = {}
    for component in pcb_report.get('selected_components',[]):
        ref = text(component.get('ref'),'PCB reference',64)
        if ref in selected:
            raise ValueError('Duplicate selected PCB reference')
        for key in ('value','footprint'):
            text(component.get(key),key)
        selected[ref] = component
    if not selected:
        raise ValueError('No selected PCB components')
    observed = {ref:[] for ref in selected}
    for symbol in children(leaf,'symbol'):
        props = properties(symbol)
        ref = props.get('Reference')
        if ref not in selected:
            continue
        instances = single(symbol,'instances')
        projects = [entry for entry in children(instances,'project') if len(entry) >= 2 and entry[1] == project]
        if len(projects) != 1:
            raise ValueError('Selected symbol has no unique project instance')
        paths = []
        for entry in children(projects[0],'path'):
            if len(entry) < 2 or not isinstance(entry[1],str):
                raise ValueError('Malformed symbol instance path')
            if scalar(entry,'reference') == ref:
                paths.append(entry[1])
        if paths.count(instance_path) != 1:
            raise ValueError('Selected symbol has no unique matching sheet instance')
        observed[ref].append({'value':text(props.get('Value'),'symbol value'),
            'footprint':text(props.get('Footprint'),'symbol footprint'),
            'part_number':text(props['PN'],'part number') if props.get('PN') else None,
            'matching_instance_path':instance_path,'other_instance_paths':sorted(p for p in paths if p != instance_path)})
    findings, comparisons = [], []
    if root_rev is None or leaf_rev is None or root_rev != leaf_rev:
        findings.append({'code':'SCHEMATIC_REVISION_REQUIRES_REVIEW','root_revision':root_rev,'leaf_revision':leaf_rev})
    for ref, pcb in sorted(selected.items()):
        records = observed[ref]
        if len(records) != 1:
            findings.append({'code':'SELECTED_SYMBOL_MISSING_OR_AMBIGUOUS','ref':ref,'occurrences':len(records)})
            comparisons.append({'ref':ref,'pcb':{key:pcb[key] for key in ('value','footprint')},'schematic':records,'fields_match':False})
            continue
        sch = records[0]
        equal = all(sch[key] == pcb[key] for key in ('value','footprint'))
        comparisons.append({'ref':ref,'pcb':{key:pcb[key] for key in ('value','footprint')},'schematic':sch,'fields_match':equal})
        if not equal:
            findings.append({'code':'PCB_SCHEMATIC_FIELD_MISMATCH','ref':ref})
        stem = re.match(r'^STM32[A-Z][A-Z0-9]{3}',sch['value'].upper())
        if stem:
            part = re.match(r'^STM32[A-Z][A-Z0-9]{3}',(sch['part_number'] or '').upper())
            if part is None or stem.group() != part.group():
                findings.append({'code':'MCU_SYMBOL_PART_REQUIRES_REVIEW','ref':ref,
                    'symbol_value':sch['value'],'part_number':sch['part_number']})
        if sch['other_instance_paths']:
            findings.append({'code':'ADDITIONAL_SYMBOL_INSTANCE_PATHS','ref':ref,'paths':sch['other_instance_paths']})
    return {'format':'tunuino-component-comparison-v1','family':pcb_report['family'],
        'validation':'unreviewed','result':'requires_review' if findings else 'selected_fields_match',
        'firmware_pin_definitions_generated':False,'mcu_compatibility_validated':False,
        'scope':'selected component fields and one direct sheet instance; no schematic net or copper comparison',
        'source':{'repository':source['repository'],'commit':source['commit'],
            'pcb':source,'root_schematic':root_source,'leaf_schematic':sheet_source},
        'root_schematic_revision':root_rev,'leaf_schematic_revision':leaf_rev,
        'pcb_revision':pcb_report.get('revision'),'comparisons':comparisons,'findings':findings}


def bounded(path):
    with path.open('rb') as source:
        data=source.read(MAX_BYTES+1)
    if len(data)>MAX_BYTES:
        raise ValueError('Source exceeds size limit')
    return data


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pcb-report',type=Path,required=True)
    parser.add_argument('--root-schematic',type=Path,required=True)
    parser.add_argument('--root-path',required=True)
    parser.add_argument('--sheet-schematic',type=Path,required=True)
    parser.add_argument('--sheet-path',required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    try:
        report=compare(json.loads(bounded(args.pcb_report)),bounded(args.root_schematic),bounded(args.sheet_schematic),
            root_path=args.root_path,sheet_path=args.sheet_path)
        encoded=json.dumps(report,ensure_ascii=False,indent=2)+'\n'
        with args.output.open('x',encoding='utf-8',newline='\n') as destination:
            destination.write(encoded)
    except (OSError,ValueError,KeyError,TypeError) as error:
        parser.exit(1,f'Comparison failed: {error}\n')


if __name__=='__main__':
    main()
