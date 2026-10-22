#!/usr/bin/env python3
# Copyright (c) 2026 Tunuino.
"""Import selected connector net membership; never generate firmware pins."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import xml.etree.ElementTree as ET

MAX_BYTES = 16 * 1024 * 1024


def text(value, label, limit=512):
    if not isinstance(value, str) or not value or len(value) > limit or any(ord(c) < 32 for c in value):
        raise ValueError(f"Invalid {label}")
    return value


def section(parent, name):
    matches = parent.findall(name)
    if len(matches) != 1:
        raise ValueError(f"Expected one {name} section")
    return matches[0]


def import_netlist(data, *, family, revision, repository, commit, source_path, connectors, targets):
    if family not in ("proteus", "uaefi"):
        raise ValueError("Unknown board family")
    text(revision, "revision", 64)
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9_.-]+", repository):
        raise ValueError("Invalid repository")
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("An exact lowercase Git commit SHA is required")
    path = PurePosixPath(text(source_path, "source path"))
    if path.is_absolute() or ".." in path.parts or "\\" in source_path or ":" in source_path:
        raise ValueError("Source path must be repository relative")
    if not connectors or len(set(connectors)) != len(connectors) or len(set(targets)) != len(targets):
        raise ValueError("Select unique connector and target references")
    if set(connectors) & set(targets):
        raise ValueError("Connector and target roles must be distinct")
    if not data or len(data) > MAX_BYTES:
        raise ValueError("Netlist exceeds size limit or is empty")
    try:
        xml = data.decode("utf-8-sig")
        if "\0" in xml or re.search(r"<!\s*(?:DOCTYPE|ENTITY)\b", xml, re.IGNORECASE):
            raise ValueError("DTD/entities and non-UTF-8 XML are unsupported")
        root = ET.fromstring(xml)
    except (UnicodeError, ET.ParseError) as error:
        raise ValueError("Invalid UTF-8 XML netlist") from error
    if root.tag != "export" or root.get("version") not in ("D", "E"):
        raise ValueError("Expected KiCad XML export version D or E")
    design = section(root, "design")
    roots = [sheet for sheet in design.findall("sheet") if sheet.get("number") == "1"]
    if len(roots) != 1 or roots[0].findtext("title_block/rev") != revision:
        raise ValueError("Requested revision differs from root schematic revision")
    components = {}
    entries = section(root, "components").findall("comp")
    if len(entries) > 20000:
        raise ValueError("Too many components")
    for comp in entries:
        ref = text(comp.get("ref"), "component reference", 64)
        if ref in components:
            raise ValueError(f"Duplicate component {ref}")
        components[ref] = comp
    selected = sorted(set(connectors) | set(targets))
    for ref in selected:
        if ref not in components:
            raise ValueError(f"Unknown selected reference {ref}")
    records, selected_nets = [], []
    endpoints, names, codes = set(), set(), set()
    nets = section(root, "nets").findall("net")
    if len(nets) > 50000:
        raise ValueError("Too many nets")
    for net in nets:
        name = text(net.get("name"), "net name")
        code = text(net.get("code"), "net code", 12)
        if not code.isascii() or not code.isdecimal() or int(code) < 1 or name in names or int(code) in codes:
            raise ValueError("Invalid or duplicate net name/code")
        names.add(name); codes.add(int(code))
        nodes = []
        for node in net.findall("node"):
            ref = text(node.get("ref"), "node reference", 64)
            pin = text(node.get("pin"), "pin number", 128)
            if ref not in components or (ref, pin) in endpoints:
                raise ValueError("Unknown component or repeated physical pin")
            endpoints.add((ref, pin))
            if len(endpoints) > 200000:
                raise ValueError("Too many net nodes")
            record = {"ref": ref, "pin": pin}
            for key in ("pinfunction", "pintype"):
                if node.get(key):
                    record[key] = text(node.get(key), key)
            nodes.append(record)
        if not nodes:
            raise ValueError("Empty net")
        # Only exact same-net membership; no traversal through components.
        for node in nodes:
            if node["ref"] in connectors:
                records.append({"connector": node["ref"], "pin": node["pin"], "net": name})
        if any(node["ref"] in connectors for node in nodes):
            selected_nets.append({"name": name, "nodes": sorted(nodes, key=lambda n: (n["ref"], n["pin"])),
                "selected_target_nodes": sorted([n for n in nodes if n["ref"] in targets],
                    key=lambda n: (n["ref"], n["pin"]))})
    return {"format": "tunuino-net-membership-v1", "family": family, "revision": revision,
        "validation": "unreviewed", "firmware_pin_definitions_generated": False,
        "scope": "selected connector pins listed in exported nets; same-net membership only",
        "source": {"repository": repository, "commit": commit, "path": source_path,
            "sha256": hashlib.sha256(data).hexdigest(),
            "git_blob_sha1": hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()},
        "source_counts": {"components": len(components), "nets": len(nets), "nodes": len(endpoints)},
        "selected_components": [{"ref": ref, "role": "connector" if ref in connectors else "target",
            "value": text(components[ref].findtext("value"), "component value"),
            "footprint": text(components[ref].findtext("footprint"), "footprint") if components[ref].findtext("footprint") else ""} for ref in selected],
        "nets": sorted(selected_nets, key=lambda n: n["name"]),
        "pins": sorted(records, key=lambda p: (p["connector"], p["pin"]))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("netlist", type=Path)
    for name in ("family", "revision", "repository", "commit", "source-path"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--connector", action="append", required=True)
    parser.add_argument("--target", action="append", default=[])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        with args.netlist.open("rb") as source:
            data = source.read(MAX_BYTES + 1)
        report = import_netlist(data, family=args.family, revision=args.revision,
            repository=args.repository, commit=args.commit, source_path=args.source_path,
            connectors=args.connector, targets=args.target)
        # Validate and serialize before exclusive creation; preserve existing reports.
        encoded = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        with args.output.open("x", encoding="utf-8", newline="\n") as output:
            output.write(encoded)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Import failed: {error}\n")


if __name__ == "__main__":
    main()
