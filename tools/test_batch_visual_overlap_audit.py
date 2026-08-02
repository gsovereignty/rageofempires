#!/usr/bin/env python3

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

from PIL import Image


MODULE_PATH = Path(__file__).with_name("batch_visual_overlap_audit.py")
SPEC = importlib.util.spec_from_file_location("batch_visual_overlap_audit", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class BatchVisualOverlapAuditTests(unittest.TestCase):
    def build_fixture(self, directory: Path) -> Path:
        terrain = Image.new("RGBA", (8, 7), (30, 100, 40, 255))
        sprite = Image.new("RGBA", (4, 4), (180, 60, 30, 255))
        clean = terrain.copy()
        clean.alpha_composite(sprite, (2, 2))
        overlap = clean.copy()
        for y in (3, 4):
            for x in (3, 4):
                overlap.putpixel((x, y), terrain.getpixel((x, y)))
        for name, image in (
            ("terrain.png", terrain), ("sprite.png", sprite),
            ("clean.png", clean), ("overlap.png", overlap),
        ):
            image.save(directory / name)
        manifest = {
            "schema_version": 1,
            "thresholds": {"minimum_area": 2},
            "cases": [
                {"id": "z-clean", "actual": "clean.png", "terrain": "terrain.png",
                 "sprite": "sprite.png", "x": 2, "y": 2,
                 "metadata": {"unit": "Town Center", "tick": 0}},
                {"id": "a-overlap", "actual": "overlap.png", "terrain": "terrain.png",
                 "sprite": "sprite.png", "screen_x": 3, "screen_y": 3,
                 "anchor_x": 1, "anchor_y": 1,
                 "metadata": {"unit": "Town Center", "tick": 8}},
            ],
        }
        path = directory / "manifest.json"
        path.write_text(json.dumps(manifest))
        return path

    def test_batch_writes_deterministic_whole_corpus_review(self):
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            manifest = self.build_fixture(directory)
            first = directory / "first"
            second = directory / "second"
            report = MODULE.run_batch(manifest, first)
            MODULE.run_batch(manifest, second)

            self.assertEqual(report["summary"], {"total": 2, "flagged": 1, "clean": 1})
            self.assertEqual([case["id"] for case in report["cases"]], ["a-overlap", "z-clean"])
            self.assertEqual((first / "report.json").read_bytes(), (second / "report.json").read_bytes())
            self.assertEqual((first / "review.html").read_bytes(), (second / "review.html").read_bytes())
            page = (first / "review.html").read_text()
            self.assertIn("Download decisions JSON", page)
            self.assertIn("Terrain only", page)
            self.assertIn("Expected sprite", page)
            self.assertIn("a-overlap", page)
            self.assertTrue((first / "assets/a-overlap-annotated.png").is_file())

    def test_duplicate_identifier_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            manifest_path = self.build_fixture(directory)
            manifest = json.loads(manifest_path.read_text())
            manifest["cases"][1]["id"] = manifest["cases"][0]["id"]
            manifest_path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, "duplicate case id"):
                MODULE.run_batch(manifest_path, directory / "output")


if __name__ == "__main__":
    unittest.main()
