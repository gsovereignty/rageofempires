#!/usr/bin/env python3

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image


MODULE_PATH = Path(__file__).with_name("visual_overlap_audit.py")
SPEC = importlib.util.spec_from_file_location("visual_overlap_audit", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


class VisualOverlapAuditTests(unittest.TestCase):
    def images(self):
        terrain = Image.new("RGBA", (12, 10), (40, 90, 35, 255))
        sprite = Image.new("RGBA", (7, 6), (210, 40, 30, 255))
        actual = terrain.copy()
        actual.alpha_composite(sprite, (3, 2))
        return actual, terrain, sprite

    def test_terrain_wedge_is_detected_bounded_and_annotated(self):
        actual, terrain, sprite = self.images()
        for y in range(3, 8):
            for x in range(4, 4 + y - 2):
                actual.putpixel((x, y), terrain.getpixel((x, y)))
        report, annotated = MODULE.audit_images(
            actual, terrain, sprite, (3, 2), minimum_area=2
        )
        self.assertEqual(report["status"], "overlap_detected")
        self.assertEqual(report["component_count"], 1)
        self.assertEqual(
            report["components"][0]["bounds"],
            {"x": 4, "y": 3, "width": 5, "height": 5},
        )
        self.assertEqual(annotated.getpixel((4, 3)), (255, 0, 0, 255))
        self.assertEqual(annotated.getpixel((5, 6)), actual.getpixel((5, 6)))
        self.assertEqual(annotated.getpixel((3, 3)), actual.getpixel((3, 3)))

    def test_transparent_hole_and_tiny_noise_are_ignored(self):
        actual, terrain, sprite = self.images()
        sprite.putpixel((2, 2), (0, 0, 0, 0))
        sprite.putpixel((4, 2), (210, 40, 30, 40))
        actual.putpixel((5, 4), terrain.getpixel((5, 4)))
        actual.putpixel((7, 4), terrain.getpixel((7, 4)))
        actual.putpixel((8, 6), terrain.getpixel((8, 6)))
        report, _ = MODULE.audit_images(
            actual, terrain, sprite, (3, 2), minimum_area=2
        )
        self.assertEqual(report["candidate_pixels"], 1)
        self.assertEqual(report["component_count"], 0)
        self.assertEqual(report["status"], "pass")

    def test_no_overlap_passes(self):
        actual, terrain, sprite = self.images()
        report, annotated = MODULE.audit_images(actual, terrain, sprite, (3, 2))
        self.assertEqual(report["component_count"], 0)
        self.assertEqual(annotated.tobytes(), actual.tobytes())

    def test_negative_offset_is_bounds_safe(self):
        terrain = Image.new("RGBA", (4, 4), (10, 30, 50, 255))
        sprite = Image.new("RGBA", (4, 4), (220, 150, 30, 255))
        actual = terrain.copy()
        report, _ = MODULE.audit_images(
            actual, terrain, sprite, (-2, -1), minimum_area=1
        )
        self.assertEqual(report["opaque_pixels_compared"], 6)
        self.assertEqual(report["components"][0]["bounds"], {
            "x": 0, "y": 0, "width": 2, "height": 3
        })

    def test_cli_json_is_deterministic_and_exit_status_is_useful(self):
        actual, terrain, sprite = self.images()
        actual.putpixel((4, 3), terrain.getpixel((4, 3)))
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            paths = [directory / name for name in ("actual.png", "terrain.png", "sprite.png")]
            for image, path in zip((actual, terrain, sprite), paths):
                image.save(path)
            outputs = []
            for suffix in ("one", "two"):
                json_path = directory / f"{suffix}.json"
                result = subprocess.run(
                    [sys.executable, str(MODULE_PATH), *(str(path) for path in paths),
                     "--screen-x", "4", "--screen-y", "4",
                     "--anchor-x", "1", "--anchor-y", "2",
                     "--minimum-area", "1", "--json-output", str(json_path),
                     "--annotated-output", str(directory / f"{suffix}.png")],
                    check=False, capture_output=True, text=True,
                )
                self.assertEqual(result.returncode, 1, result.stderr)
                outputs.append(json_path.read_text())
            self.assertEqual(outputs[0], outputs[1])
            self.assertEqual(json.loads(outputs[0])["placement"], {"x": 3, "y": 2})


if __name__ == "__main__":
    unittest.main()
