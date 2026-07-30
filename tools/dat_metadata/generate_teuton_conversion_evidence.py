#!/usr/bin/env python3
"""Extract pinned Teuton conversion evidence from legal local assets."""

import argparse
import hashlib
import json
import re
from pathlib import Path

EXPECTED_SHA256 = "e23272e21014fb281f71a21ef96a6437ab8b322f4978fd4998be835be219edcc"


def extract(metadata, executable):
    civilizations = metadata["civilizations"]
    teutons = civilizations[4]
    match = re.search(r"\bbonus_effect: Some\((\d+)\)", teutons["record"])
    if not match:
        raise ValueError("Teuton civilization record has no decoded bonus_effect")
    effect_id = int(match.group(1))
    effect = next(item for item in metadata["effects"] if item["id"] == effect_id)
    digest = hashlib.sha256(executable.read_bytes()).hexdigest()
    if digest != EXPECTED_SHA256:
        raise ValueError(f"unexpected executable SHA-256: {digest}")
    return {
        "schema": 1,
        "inputs": {
            "executable": str(executable),
            "executable_sha256": digest,
            "dat_version": "VER 5.7",
        },
        "team_bonus": {
            "civilization_id": 4,
            "dat_name": teutons["name"],
            "bonus_effect_id": effect_id,
            "effect_commands": effect["commands"],
            "dispatcher": {
                "effect_dispatch_va": "0x4f73e0",
                "resource_mode_dispatch_va": "0x5839c0",
                "set_wrapper_va": "0x5a4d70",
                "set_mutation_va": "0x582860",
                "add_wrapper_va": "0x5a4e90",
                "add_mutation_va": "0x582940",
                "mode_semantics": {
                    "0": "resource[attribute_id] = amount",
                    "nonzero": "resource[attribute_id] += amount",
                },
            },
            "per_application_result_from_zero": {
                "77": 2.0,
                "178": 1.0,
                "179": 2.0,
            },
            "duplicate_application": {
                "status": "exact",
                "reason": (
                    "team initialization builds a byte bitmap keyed by civilization "
                    "ID, marks own and relation-3 player civilizations, then applies "
                    "each marked civilization record +0x2c bonus effect once"
                ),
                "player_field_store_va": "0x585c25",
                "player_field_offset": "0x16c",
                "later_player_field_reads": [],
                "team_initialization_va": "0x54a740",
                "own_civilization_effect_apply_va": "0x54a7a0",
                "eligible_relation_function_va": "0x581dc0",
                "eligible_relation_value": 3,
                "civilization_seen_bitmap_allocation_va": "0x54a76f",
                "civilization_bonus_effect_offset": "0x2c",
                "distinct_bonus_effect_apply_va": "0x54a856",
                "semantics": (
                    "each distinct eligible civilization bonus applies once per "
                    "recipient, including recipient civilization"
                ),
            },
            "serialization": {
                "player_resource_transfer_va": "0x582aa0",
                "count_offset": "0xa4",
                "array_pointer_offset": "0xa8",
                "transfer": "count then count contiguous IEEE-754 float32 values",
                "post_load_team_effect_reapplication": "unproved",
            },
        },
        "conversion_check": {
            "class_rtti": ".?AVTRIBE_Action_Convert@@",
            "class_rtti_file_offset": "0x416284",
            "type_descriptor_va": "0x81627c",
            "complete_object_locator_va": "0x80534c",
            "vtable_va": "0x772acc",
            "function_va": "0x413a80",
            "rand_call_va": "0x413e2c",
            "resistance_multiply_va": "0x413f19",
            "threshold_compare_va": "0x413feb",
            "float_to_integer_helper_va": "0x72421c",
            "resources": {
                "77": "target roll-resistance accumulator",
                "176": "converter minimum-time delta",
                "177": "converter maximum-time delta",
                "178": "target minimum-time delta",
                "179": "target maximum-time delta",
                "180": "building base minimum time",
                "181": "building base maximum time",
                "182": "building base chance",
            },
            "class_resistance": 3.0,
            "resistant_target_classes": [2, 20, 21, 22, 53],
            "resistant_target_flag_values": [2, 10],
            "class_resistance_exempt_converter_class": 53,
            "special_unit_resistance": 8.0,
            "special_unit_ids": [448, 546, 441, 751, 752],
            "forced_failure_threshold": -1000,
            "forced_success_threshold": 1000,
            "minimum_boundary": "elapsed < minimum forces failure",
            "maximum_boundary": "elapsed >= maximum forces success",
            "success_comparison": "scaled_roll <= threshold",
        },
        "classification": {
            "formula": "exact executable data flow",
            "effect_payload": "exact DAT record",
            "effect_mode": "exact executable data flow",
            "resource_array_serialization": "exact executable data flow",
            "duplicate_team_application": "exact executable data flow",
            "post_load_team_effect_reapplication": "unproved",
        },
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata", type=Path)
    parser.add_argument("executable", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    evidence = extract(json.loads(args.metadata.read_text()), args.executable)
    args.output.write_text(json.dumps(evidence, indent=2) + "\n")


if __name__ == "__main__":
    main()
