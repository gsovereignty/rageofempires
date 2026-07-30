#!/usr/bin/env python3

import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path


PATH = Path(__file__).with_name("audit_hud_contract.py")
SPEC = importlib.util.spec_from_file_location("audit_hud_contract", PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


def slp(width=1280, height=1024):
    payload = bytearray(128)
    payload[:4] = b"2.0N"
    struct.pack_into("<I", payload, 4, 1)
    struct.pack_into(
        "<IIIIiiii", payload, 32,
        96, 100, 0, 0, width, height, 0, 0
    )
    return bytes(payload)


def drs(payload):
    data = bytearray(100 + len(payload))
    struct.pack_into("<II", data, 56, 1, 0)
    struct.pack_into("<4sII", data, 64, b" pls", 76, 1)
    struct.pack_into("<III", data, 76, MODULE.HUD_SLP, 100, len(payload))
    data[100:] = payload
    return bytes(data)


class HudContractTests(unittest.TestCase):
    def test_exact_hud_metadata(self):
        result = MODULE.slp_metadata(slp())
        self.assertEqual(result["frame_count"], 1)
        self.assertEqual(result["frames"][0]["width"], 1280)
        self.assertEqual(result["frames"][0]["height"], 1024)

    def test_drs_bounds_fail_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "interfac.drs"
            broken = bytearray(drs(slp()))
            struct.pack_into("<I", broken, 84, len(broken) + 1)
            path.write_bytes(broken)
            with self.assertRaisesRegex(ValueError, "outside archive"):
                MODULE.drs_resource(path, MODULE.HUD_SLP)

    def test_incomplete_mapping_retains_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            interface = root / "interfac.drs"
            interface.write_bytes(drs(slp()))
            executable = root / "game.exe"
            executable.write_bytes(
                b"game_b%d.slp\0map1024.bmp\0Game Screen\0"
            )
            report = MODULE.make_catalog(interface, executable)
            self.assertFalse(
                report["renderer_decision"]["exact_layout_enabled"]
            )
            self.assertTrue(
                report["renderer_decision"]["candidate_sheet_available"]
            )
            self.assertFalse(
                report["renderer_decision"]["candidate_sheet_promoted"]
            )
            self.assertFalse(
                report["unlinked_candidate_sheet"]["game_background_compatible"]
            )
            self.assertIn("panel_rectangles", report["unproved"])
            relative = report["executable_relative_layout"]
            self.assertEqual(
                relative["classification"],
                "unproved_non_pinned_executable",
            )
            self.assertEqual(relative["command_grid"]["columns"], 5)
            self.assertEqual(relative["command_grid"]["rows"], 3)
            self.assertEqual(relative["command_grid"]["count"], 15)
            self.assertIn("stored_bottom_height",
                          relative["vertical_layout"]["bottom"])
            self.assertEqual(
                report["renderer_resolution"]["classification"],
                "reconstruction_policy",
            )
            self.assertFalse(
                report["renderer_decision"]["exact_relative_contract_enabled"]
            )
            self.assertFalse(
                report["renderer_decision"]["exact_layout_enabled"]
            )
            self.assertEqual(report["button_chrome"]["normal_frame"], 36)
            self.assertEqual(report["button_chrome"]["pressed_frame"], 37)
            self.assertIn("generic", report["button_chrome"]["limits"])

    def test_pinned_identity_enables_relative_contract(self):
        original = MODULE.PINNED_EXECUTABLE_SHA256
        try:
            with tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                interface = root / "interfac.drs"
                interface.write_bytes(drs(slp()))
                executable = root / "game.exe"
                executable.write_bytes(b"pinned fixture")
                MODULE.PINNED_EXECUTABLE_SHA256 = MODULE.sha256(
                    executable.read_bytes()
                )
                report = MODULE.make_catalog(interface, executable)
                self.assertEqual(
                    report["executable_relative_layout"]["classification"],
                    "exact_relative",
                )
                self.assertEqual(
                    report["button_chrome"]["classification"],
                    "exact_relative",
                )
                self.assertTrue(
                    report["renderer_decision"][
                        "exact_relative_contract_enabled"
                    ]
                )
        finally:
            MODULE.PINNED_EXECUTABLE_SHA256 = original


if __name__ == "__main__":
    unittest.main()
