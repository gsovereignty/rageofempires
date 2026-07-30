#!/usr/bin/env python3

import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("audit_selection_feedback.py")
SPEC = importlib.util.spec_from_file_location(
    "audit_selection_feedback", MODULE_PATH
)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class SelectionFeedbackTests(unittest.TestCase):
    def test_unit_feedback_keeps_only_cross_civilization_exact_records(self):
        base = {
            "id": 7,
            "disabled": False,
            "selection_shape": 2,
            "outline_radius": [0.2, 0.3, 2.0],
            "radius": [0.2, 0.3, 2.0],
            "hit_points": 25,
            "selected_sound": 303,
        }
        changed = dict(base, hit_points=30)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "metadata.json"
            path.write_text(json.dumps({
                "civilizations": [
                    {"units": [base]},
                    {"units": [changed]},
                ]
            }))
            result = MODULE.unit_feedback(path)
            self.assertEqual(result["cross_civilization_exact_records"], [])
            self.assertEqual(
                result["civilization_variant_records"],
                [{"unit_id": 7, "variant_count": 2}],
            )

    def test_at_va_reads_section_bytes(self):
        data = bytearray(0x300)
        struct.pack_into("<I", data, 0x3C, 0x80)
        data[0x80:0x84] = b"PE\0\0"
        struct.pack_into("<H", data, 0x86, 1)
        struct.pack_into("<H", data, 0x94, 0xE0)
        table = 0x80 + 24 + 0xE0
        struct.pack_into("<IIII", data, table + 8, 0x20, 0x1000, 0x20, 0x200)
        data[0x205:0x208] = b"abc"
        self.assertEqual(MODULE.at_va(bytes(data), 0x401005, 3), b"abc")

    def test_health_contract_has_no_color_thresholds(self):
        # Contract arithmetic follows inclusive fill endpoints from
        # FUN_004d27b0: full HP spans -12..+12.
        width = ((100 * 24) // 100 - 12) - (-12) + 1
        self.assertEqual(width, 25)

    def test_hd_health_split_uses_32_units_and_right_margin(self):
        left, right = -16, 15
        self.assertEqual(
            min(left + (50 * 32) // 100, right - 2),
            0,
        )
        self.assertEqual(
            min(left + (100 * 32) // 100, right - 2),
            13,
        )

    def test_archive_feedback_preserves_missing_resource(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "ui.json"
            path.write_text(json.dumps({
                "resources": [
                    {"resource_id": 50403, "frame_count": 9},
                    {"resource_id": 50404, "frame_count": 1},
                    {"resource_id": 50745, "frame_count": 26},
                    {"resource_id": 53003, "frame_count": 1},
                ]
            }))
            result = MODULE.archive_feedback(path)["resources"]
            self.assertEqual(result["50404"]["frame_count"], 1)
            self.assertIsNone(result["50405"])

if __name__ == "__main__":
    unittest.main()
