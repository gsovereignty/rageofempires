import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("generate_cursor_catalog.py")
SPEC = importlib.util.spec_from_file_location("cursor_catalog", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def synthetic_slp():
    header = bytearray(32 + 32)
    header[:4] = b"2.0N"
    struct.pack_into("<I", header, 4, 1)
    command_table = len(header)
    outline_table = command_table + 8
    struct.pack_into(
        "<IIIIiiii", header, 32,
        command_table, outline_table, 0, 0, 24, 32, 3, 7
    )
    return bytes(header) + bytes(8 + 32 * 4)


def synthetic_drs(payload):
    header = bytearray(64)
    header[40:42] = b"1."
    struct.pack_into("<i", header, 56, 1)
    table = b"pls " + struct.pack("<ii", 76, 1)
    entry = struct.pack("<iii", 51000, 88, len(payload))
    return bytes(header) + table + entry + payload


class CursorCatalogParserTests(unittest.TestCase):
    def test_exact_dimensions_and_hotspot_from_slp_header(self):
        frame = MODULE.slp_frames(synthetic_slp())[0]
        self.assertEqual(
            (frame["width"], frame["height"]), (24, 32)
        )
        self.assertEqual(
            (frame["hotspot_x"], frame["hotspot_y"]), (3, 7)
        )
        self.assertIsNone(frame["context_state"])
        self.assertEqual(frame["context_state_classification"], "unknown")

    def test_drs_resource_bounds_are_validated(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "interfac.drs"
            path.write_bytes(synthetic_drs(synthetic_slp()))
            self.assertEqual(
                MODULE.drs_resource(path, "slp", 51000)[:4], b"2.0N"
            )
            broken = bytearray(path.read_bytes())
            struct.pack_into("<i", broken, 80, len(broken) + 1)
            path.write_bytes(broken)
            with self.assertRaisesRegex(ValueError, "payload outside"):
                MODULE.drs_resource(path, "slp", 51000)


class CheckedInCursorCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(
            Path("generated/cursor_catalog.json").read_text()
        )

    def test_all_nineteen_live_frames_are_exactly_cataloged(self):
        self.assertEqual(
            self.data["summary"],
            {
                "frame_count": 19,
                "frames_with_exact_dimensions": 19,
                "frames_with_exact_hotspots": 19,
                "frames_with_proved_context_state": 2,
            },
        )
        self.assertEqual(
            [frame["frame_index"] for frame in self.data["frames"]],
            list(range(19)),
        )

    def test_known_metadata_extremes_remain_pinned(self):
        self.assertEqual(
            self.data["source"]["interfac_drs_sha256"],
            "cb9e4d0f59d6cdb7af70da38cc910d0c33d210fe2d2a73dea17ef52a4ac8826e",
        )
        self.assertEqual(
            self.data["source"]["slp_payload_sha256"],
            "44c68278a1c1dc0677549e9747d3503b3a538d05f643ccc904b62785cab8512b",
        )
        frames = self.data["frames"]
        self.assertEqual(
            (frames[0]["width"], frames[0]["height"],
             frames[0]["hotspot_x"], frames[0]["hotspot_y"]),
            (24, 32, 0, 0),
        )
        self.assertEqual(
            (frames[18]["width"], frames[18]["height"],
             frames[18]["hotspot_x"], frames[18]["hotspot_y"]),
            (45, 41, 23, 40),
        )
        self.assertEqual(frames[0]["context_state"], "normal")
        self.assertEqual(frames[6]["context_state"], "modal_busy")
        self.assertEqual(
            sum(
                frame["context_state_classification"] ==
                "exact_hd_executable_selector"
                for frame in frames
            ),
            2,
        )

    def test_hd_load_does_not_claim_classic_state_mapping(self):
        evidence = self.data["executable_evidence"]
        self.assertEqual(evidence["resource_id"], 51000)
        self.assertEqual(
            evidence["resource_binding_classification"],
            "exact_hd_executable_load",
        )
        self.assertEqual(
            evidence["state_to_frame_contract"],
            "partial_exact_hd_executable",
        )
        self.assertEqual(evidence["classic_aoc_state_mapping"], "unknown")
        self.assertEqual(
            [(item["state"], item["frame_index"])
             for item in evidence["proved_selectors"]],
            [("normal", 0), ("modal_busy", 6)],
        )
        self.assertIn("attack", evidence["unproved_gameplay_states"])


if __name__ == "__main__":
    unittest.main()
