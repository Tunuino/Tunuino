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
labels = re.findall(r'\(label "([^"]+)" \(at ([\d.]+) ([\d.]+) 0\)', sch)
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


# Validate the approved six-cylinder/stepper hardware baseline across documents.
from collections import Counter
names = Counter(r["net"] for r in rows)
for prefix, count in (("INJ", 6), ("IGN", 6)):
    expected = {f"{prefix}{i}" for i in range(1, count + 1)}
    actual_names = {n for n in names if re.fullmatch(rf"{prefix}\d+", n)}
    assert actual_names == expected, (prefix, actual_names, expected)
    assert all(names[n] == 1 for n in expected), f"Duplicate {prefix} channel"
expected_stepper = {"STEPPER_A1", "STEPPER_A2", "STEPPER_B1", "STEPPER_B2"}
assert {n for n in names if n.startswith("STEPPER_")} == expected_stepper
assert all(names[n] == 1 for n in expected_stepper)

with (HERE / "circuit-interfaces.csv").open(newline="", encoding="utf-8") as f:
    circuits = list(csv.DictReader(f))
assert len({(r["connector"], r["pin"]) for r in circuits}) == len(circuits), "Duplicate circuit pin"
for circuit in circuits:
    key = (circuit["connector"], int(circuit["pin"]))
    assert actual[key] == circuit["signal"], (key, circuit["signal"], actual[key])
assert {r["signal"] for r in circuits if r["block"] == "stepper"} == expected_stepper

board = (HERE.parent / "firmware/platform/stm32f429/board.h").read_text(encoding="utf-8")
for macro, expected in (("TUNUINO_INJECTOR_COUNT", 6), ("TUNUINO_IGNITION_COUNT", 6), ("TUNUINO_STEPPER_PHASE_COUNT", 4), ("TUNUINO_DBW_ENABLED", 0)):
    assert re.search(rf"^#define {macro} {expected}u$", board, re.MULTILINE), macro
print("OK: six injectors, six ignitions, four stepper wires and disabled DBW match firmware and circuit register")
