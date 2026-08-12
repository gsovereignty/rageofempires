#!/usr/bin/env python3

import importlib.util
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("audit_multiplayer_screenshots.py")
SPEC = importlib.util.spec_from_file_location("screenshot_audit", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def png(path: Path, width: int, height: int, color: tuple[int, int, int]):
    def chunk(kind: bytes, value: bytes) -> bytes:
        return (struct.pack(">I", len(value)) + kind + value +
                struct.pack(">I", zlib.crc32(kind + value) & 0xffffffff))
    scanlines = b"".join(b"\0" + bytes(color) * width for _ in range(height))
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n" +
        chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) +
        chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b"")
    )


def provenance(root: Path, entity: dict):
    (root / "sprite-provenance.jsonl").write_text(json.dumps({
        "peer": "host", "frame": 3, "entities": [entity],
    }) + "\n")


class ScreenshotAuditTests(unittest.TestCase):
    def test_flags_black_frame_and_blocks_unproved_overlap(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "frames").mkdir()
            png(root / "frames" / "black.png", 8, 8, (0, 0, 0))
            provenance(root, {
                "id": 1, "source": "legacy", "expectedAssetStatus": "renderable",
                "expectedResourceIds": [10], "layers": [{"resourceId": 10}],
            })
            report = MODULE.audit(root)
            self.assertEqual(report["status"], "FAIL")
            self.assertIn("black_frame", {item["kind"] for item in report["findings"]})
            self.assertIn("overlap_evidence", {item["kind"] for item in report["findings"]})

    def test_flags_procedural_and_wrong_resource_mapping(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sprite-provenance.jsonl").write_text("\n".join((
                json.dumps({"peer": "host", "frame": 1, "entities": [{
                    "id": 2, "source": "intentional_procedural",
                }]}),
                json.dumps({"peer": "join", "frame": 1, "entities": [{
                    "id": 3, "source": "legacy", "expectedAssetStatus": "renderable",
                    "expectedResourceIds": [10], "layers": [{"resourceId": 11}],
                }]}),
            )) + "\n")
            kinds = {item["kind"] for item in MODULE.audit(root)["findings"]}
            self.assertIn("procedural_render", kinds)
            self.assertIn("wrong_resource_mapping", kinds)

    def test_flags_missing_sprite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            provenance(root, {
                "id": 4, "source": "legacy", "expectedAssetStatus": "renderable",
                "expectedResourceIds": [10], "layers": [],
            })
            kinds = {item["kind"] for item in MODULE.audit(root)["findings"]}
            self.assertIn("missing_sprite", kinds)


if __name__ == "__main__":
    unittest.main()
