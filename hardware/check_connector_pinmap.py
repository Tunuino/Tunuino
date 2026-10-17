#!/usr/bin/env python3
"""Validate J102/J103 target labels against the KiCad connector schematic."""
from pathlib import Path
import csv
import re

HERE = Path(__file__).resolve().parent
sch = (HERE / "tunuino.kicad_sch").read_text(encoding="utf-8")
with (HERE / "connector-pinmap.csv").open(newline="", encoding="utf-8") as f:
    rows = list(csv.DictReader(f))
assert len(rows) == 70, "Expected exactly 70 pinmap rows"
assert {(r["connector"], int(r["pin"])) for r in rows} == {
    (c, p) for c in ("J102", "J103") for p in range(1, 36)
}, "Missing/duplicated connector positions"
labels = re.findall(r'\\(label "([^"]+)" \\(at ([\\d.]+) ([\\d.]+) 0\\)', sch)
assert len(labels) == 70, "Expected exactly 70 schematic net labels"
actual = {}
for name, x, y in labels:
    connector = "J102" if float(x) < 150 else "J103"
    position = (float(y) - 66.82) / 2.54 + 1
    pin = round(position)
    assert 1 <= pin <= 35 and abs(position - pin) < 0.001, (connector, name, position)
    key = (connector, pin)
    assert key not in actual, f"Duplicate label position {key}"
    actual[key] = name
for row in rows:
    key = (row["connector"], int(row["pin"]))
    assert actual[key] == row["net"], (key, actual[key], row["net"])
print("OK: 70 unique J102/J103 pins match schematic labels")
