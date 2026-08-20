#!/usr/bin/env python3

import importlib.util
import json
import struct
import tempfile
import unittest
import zlib
from pathlib import Path

from PIL import Image

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
                    "renderPosition": {"x": 10, "y": 20},
                }]}),
                json.dumps({"peer": "join", "frame": 1, "entities": [{
                    "id": 3, "source": "legacy", "expectedAssetStatus": "renderable",
                    "expectedResourceIds": [10], "layers": [{"resourceId": 11}],
                }]}),
            )) + "\n")
            kinds = {item["kind"] for item in MODULE.audit(root)["findings"]}
            self.assertIn("procedural_render", kinds)
            self.assertIn("wrong_resource_mapping", kinds)

    def test_allows_multipart_layers_after_expected_primary_body(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "sprite-provenance.jsonl").write_text(json.dumps({
                "peer": "host", "frame": 1, "entities": [{
                    "id": 45,
                    "category": "building-house",
                    "source": "legacy",
                    "expectedAssetStatus": "renderable",
                    "expectedResourceIds": [2235],
                    "layers": [
                        {"resourceId": 2235},
                        {"resourceId": 425},
                        {"resourceId": 428},
                    ],
                }],
            }) + "\n")

            findings = MODULE.provenance_findings(root)

        self.assertEqual(findings, [])

    def test_flags_missing_sprite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            provenance(root, {
                "id": 4, "source": "legacy", "expectedAssetStatus": "renderable",
                "expectedResourceIds": [10], "layers": [],
                "renderPosition": {"x": 10, "y": 20},
            })
            kinds = {item["kind"] for item in MODULE.audit(root)["findings"]}
            self.assertIn("missing_sprite", kinds)

    def test_ignores_wholly_offscreen_candidate_without_draw_submission(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            provenance(root, {
                "id": 5, "source": "procedural_or_unproven",
                "expectedAssetStatus": "renderable",
                "expectedResourceIds": [2085], "layers": [],
                "renderPosition": None,
            })
            findings = MODULE.provenance_findings(root)
            self.assertEqual(findings, [])

    def test_matched_overlap_evidence_passes_and_records_exact_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            overlap = root / "overlap"
            overlap.mkdir()
            terrain = Image.new("RGB", (12, 12), (30, 90, 20))
            sprite = Image.new("RGBA", (3, 3), (220, 30, 20, 255))
            actual = terrain.copy()
            actual.paste(sprite, (4, 5), sprite)
            actual.save(overlap / "gameplay.png")
            terrain.save(overlap / "terrain.png")
            sprite.save(overlap / "sprite.png")
            (overlap / "manifest.json").write_text(json.dumps({
                "schema_version": 1,
                "cases": [{
                    "id": "unit-7-frame-3", "actual": "gameplay.png",
                    "terrain": "terrain.png", "sprite": "sprite.png",
                    "x": 4, "y": 5,
                }],
            }))
            provenance(root, {
                "id": 7, "source": "legacy", "expectedAssetStatus": "renderable",
                "expectedResourceIds": [10], "layers": [{"resourceId": 10}],
            })

            report = MODULE.audit(root)

            self.assertEqual(report["status"], "PASS")
            self.assertEqual(report["overlapCasesAudited"], 1)
            stored = json.loads((overlap / "reports" /
                                 "unit-7-frame-3" / "report.json").read_text())
            self.assertEqual(stored["placement"], {"x": 4, "y": 5})
            self.assertEqual(set(stored["evidence"]),
                             {"actual", "terrain", "sprite"})

    def test_matched_overlap_evidence_flags_terrain_over_sprite(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            overlap = root / "overlap"
            overlap.mkdir()
            terrain = Image.new("RGB", (12, 12), (30, 90, 20))
            sprite = Image.new("RGBA", (3, 3), (220, 30, 20, 255))
            terrain.save(overlap / "gameplay.png")
            terrain.save(overlap / "terrain.png")
            sprite.save(overlap / "sprite.png")
            (overlap / "manifest.json").write_text(json.dumps({
                "schema_version": 1,
                "cases": [{
                    "id": "covered-unit", "actual": "gameplay.png",
                    "terrain": "terrain.png", "sprite": "sprite.png",
                    "x": 4, "y": 5,
                }],
            }))
            provenance(root, {
                "id": 7, "source": "legacy", "expectedAssetStatus": "renderable",
                "expectedResourceIds": [10], "layers": [{"resourceId": 10}],
            })

            report = MODULE.audit(root)

            self.assertEqual(report["status"], "FAIL")
            finding = next(item for item in report["findings"]
                           if item["kind"] == "sprite_overlap")
            self.assertEqual(finding["overlapPixels"], 9)


if __name__ == "__main__":
    unittest.main()
