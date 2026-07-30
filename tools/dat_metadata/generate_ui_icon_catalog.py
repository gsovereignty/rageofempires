#!/usr/bin/env python3
"""Join represented DAT icon indices to a live interfac.drs inventory."""

import argparse
import importlib.util
import json
import re
import struct
from collections import Counter
from pathlib import Path

RESOURCES = ["food", "wood", "gold", "stone"]
COMMANDS = [
    "move", "stop", "attack", "attack_ground", "attack_move", "patrol",
    "guard", "garrison", "ungarrison", "heal", "convert",
    "build", "repair", "gather", "trade", "delete",
]

SHEET_ROLE_EVIDENCE = [
    {
        "resource_id": 50721, "role": "command_sheet",
        "classification": "exact",
        "source": (
            "AoK-HD-patched.c executable load: btncmd.shp, 0xc621"
        ),
        "note": "sheet role exact; DAT/action index to frame semantics unknown",
    },
    {
        "resource_id": 50729, "role": "technology_icons",
        "classification": "exact",
        "source": (
            "AoK-HD-patched.c FUN_0050c6b0(\"btntech.shp\",0xc629,0)"
        ),
        "note": "FUN_005c6750 passes technology +0x2c unchanged as frame",
    },
    {
        "resource_id": 50730, "role": "unit_icons",
        "classification": "exact",
        "source": (
            "AoK-HD-patched.c FUN_0050c6b0(\"ico_unit.shp\",0xc62a,0)"
        ),
        "note": "FUN_005c7560 passes ordinary-unit +0x54 unchanged as frame",
    },
    {
        "resource_id": 50731, "role": "resource_and_stat_symbols",
        "classification": "unknown",
        "source": "openage doc/media/aoc-slp-list.md",
        "note": "descriptive list is explicitly tentative",
    },
    {
        "resource_id": 50732, "role": "resource_icons",
        "classification": "unknown",
        "source": "openage doc/media/aoc-slp-list.md",
        "note": "source labels this relationship with a question mark",
    },
    {
        "resource_id": 50760, "role": "top_bar_resource_icons",
        "classification": "unknown",
        "source": "openage doc/media/aoc-slp-list.md",
        "note": "role description does not prove DAT index/frame semantics",
    },
]


def mapping_module(path):
    spec = importlib.util.spec_from_file_location("civ_matrix", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def drs_inventory(path):
    data = Path(path).read_bytes()
    if len(data) < 64 or data[40:42] != b"1.":
        raise ValueError("invalid DRS")
    table_count = struct.unpack_from("<i", data, 56)[0]
    result = []
    for table in range(table_count):
        offset = 64 + table * 12
        extension = data[offset:offset + 4][::-1].decode(
            "ascii", errors="strict"
        ).strip("\0 ").lower()
        info, count = struct.unpack_from("<ii", data, offset + 4)
        for entry in range(count):
            entry_offset = info + entry * 12
            resource_id, payload, size = struct.unpack_from(
                "<iii", data, entry_offset
            )
            if payload < 0 or size < 0 or payload + size > len(data):
                raise ValueError("DRS entry outside archive")
            frames = None
            if extension == "slp" and size >= 8:
                frames = struct.unpack_from("<I", data, payload + 4)[0]
            result.append({
                "extension": extension, "resource_id": resource_id,
                "size": size, "frame_count": frames,
            })
    return result


def item(category, name, dat_id=None, icon_id=None):
    icon_class = "exact" if icon_id is not None else "missing"
    return {
        "category": category, "name": name, "dat_id": dat_id,
        "dat_icon_id": icon_id,
        "dat_icon_classification": icon_class,
        "slp_resource_id": None, "frame_index": None,
        "asset_relationship_classification": "unknown",
        "note": (
            "DAT icon index is exact; DAT does not identify an SLP resource "
            "or frame relationship"
            if icon_id is not None else
            "represented record exposes no icon index"
        ),
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata")
    parser.add_argument("interfac_drs")
    parser.add_argument("--mapping", default="tools/dat_metadata/generate_civ_matrix.py")
    parser.add_argument("--output", default="generated/ui_icon_catalog.json")
    args = parser.parse_args()
    data = json.loads(Path(args.metadata).read_text())
    mapping = mapping_module(Path(args.mapping))
    units = {x["id"]: x for x in data["civilizations"][0]["units"]}
    techs = {x["id"]: x for x in data["techs"]}
    items = []
    for name, (dat_id, _gate) in mapping.UNIT.items():
        items.append(item("unit", name, dat_id, units[dat_id]["button_icon"]))
    for name, (dat_id, _gate) in mapping.BUILDING.items():
        items.append(item(
            "building", name, dat_id, units[dat_id]["button_icon"]
        ))
    for name, dat_id in mapping.TECH.items():
        match = re.search(r"icon_id: Some\((\d+)\)", techs[dat_id]["record"])
        items.append(item(
            "technology", name, dat_id,
            int(match.group(1)) if match else None
        ))
    items.extend(item("resource", name) for name in RESOURCES)
    items.extend(item("command", name) for name in COMMANDS)
    counts = Counter()
    for record in items:
        counts[record["dat_icon_classification"]] += 1
        counts[record["asset_relationship_classification"]] += 1
    output = {
        "schema": "aoe-ui-icon-catalog-v1",
        "source": data["source"],
        "archive": str(Path(args.interfac_drs)),
        "scope": {
            "units": len(mapping.UNIT), "buildings": len(mapping.BUILDING),
            "technologies": len(mapping.TECH),
            "resources": len(RESOURCES), "commands": len(COMMANDS),
        },
        "classification_counts": dict(sorted(counts.items())),
        "provenance": {
            "hd_executable_sha256": (
                "e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc"
            ),
            "openage_commit": "9a5a7ccbfc20c2de658fc746462cd4a69aa758ef",
            "openage_commit_date": "2026-07-04T12:38:52+02:00",
            "dat_semantics": (
                "openage datfile readers name unit/research icon_id but skip "
                "the field during conversion"
            ),
        },
        "sheet_role_evidence": SHEET_ROLE_EVIDENCE,
        "executable_dispatch_contract": {
            "technology": {
                "sheet": 50729,
                "record_field": "+0x2c signed short",
                "frame_transform": "identity",
                "evidence": "FUN_005c6750 -> FUN_00517560 -> FUN_005c5e40",
            },
            "ordinary_unit": {
                "sheet": 50730,
                "record_field": "+0x54 signed short",
                "frame_transform": "identity",
                "excluded_subtypes": [2, 10],
                "evidence": "FUN_005c7560 -> FUN_005c5e40",
            },
            "pressed": {
                "icon_frame_transform": "identity",
                "normal_chrome_frame": 36,
                "pressed_chrome_frame": 37,
                "pressed_icon_offset": [1, 1],
                "evidence": "FUN_005c5e40",
            },
            "disabled": {"classification": "unknown"},
            "page_ordering": {"classification": "unknown"},
        },
        "interfac_inventory": drs_inventory(args.interfac_drs),
        "items": items,
    }
    Path(args.output).write_text(json.dumps(output, indent=2) + "\n")


if __name__ == "__main__":
    main()
