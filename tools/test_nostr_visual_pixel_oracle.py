#!/usr/bin/env python3

import tempfile
import unittest
from pathlib import Path

from PIL import Image, ImageDraw

from nostr_visual_pixel_oracle import (
    PixelOracleError,
    composite,
    evaluate_direction_pixels,
    write_evidence,
)


def arrow(direction: str, *, flip: bool = False) -> Image.Image:
    image = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    if direction == "east":
        draw.polygon(((3, 8), (14, 8), (14, 4), (22, 12),
                      (14, 20), (14, 16), (3, 16)), fill=(240, 40, 20, 255))
    else:
        draw.polygon(((8, 3), (16, 3), (16, 14), (20, 14),
                      (12, 22), (4, 14), (8, 14)), fill=(240, 40, 20, 255))
    return image.transpose(Image.Transpose.FLIP_LEFT_RIGHT) if flip else image


class PixelDirectionOracleTests(unittest.TestCase):
    def setUp(self):
        self.background = Image.new("RGBA", (24, 24), (30, 70, 20, 255))
        self.sprites = {
            "east": arrow("east"),
            "west": arrow("east", flip=True),
            "south": arrow("south"),
        }

    def test_correct_composite_passes_with_scores_and_margin(self):
        actual = composite(self.background, self.sprites["east"])
        report, _ = evaluate_direction_pixels(
            actual=actual, background=self.background,
            expected_direction="east", sprites=self.sprites,
        )
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["bestDirection"], "east")
        self.assertGreater(report["confidenceMargin"], 1.0)
        self.assertEqual(set(report["alternativeDirectionScores"]),
                         set(self.sprites))

    def test_wrong_direction_and_horizontal_flip_fail(self):
        for actual_direction in ("west", "south"):
            with self.subTest(actual_direction=actual_direction):
                actual = composite(
                    self.background, self.sprites[actual_direction]
                )
                report, _ = evaluate_direction_pixels(
                    actual=actual, background=self.background,
                    expected_direction="east", sprites=self.sprites,
                )
                self.assertEqual(report["verdict"], "FAIL")
                self.assertEqual(report["bestDirection"], actual_direction)

    def test_foreground_occlusion_is_excluded_without_hiding_direction(self):
        actual = composite(self.background, self.sprites["east"])
        occluder = Image.new("RGBA", (4, 24), (95, 80, 55, 255))
        actual.alpha_composite(occluder, (0, 0))
        report, _ = evaluate_direction_pixels(
            actual=actual, background=self.background,
            expected_direction="east", sprites=self.sprites,
        )
        self.assertEqual(report["verdict"], "PASS")
        self.assertEqual(report["bestDirection"], "east")
        self.assertGreaterEqual(report["discriminatingPixels"], 24)

    def test_ambiguous_crop_blocks(self):
        transparent = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
        sprites = {"east": transparent, "west": transparent.copy()}
        report, _ = evaluate_direction_pixels(
            actual=self.background, background=self.background,
            expected_direction="east", sprites=sprites,
        )
        self.assertEqual(report["verdict"], "BLOCKED")

    def test_evidence_images_and_report_are_retained(self):
        actual = composite(self.background, self.sprites["west"])
        report, images = evaluate_direction_pixels(
            actual=actual, background=self.background,
            expected_direction="east", sprites=self.sprites,
        )
        with tempfile.TemporaryDirectory() as directory:
            retained = write_evidence(Path(directory), report, images)
            self.assertTrue((Path(directory) / "report.json").is_file())
            for path in retained["images"].values():
                self.assertTrue((Path(directory) / path).is_file())

    def test_invalid_inputs_fail_closed(self):
        with self.assertRaises(PixelOracleError):
            evaluate_direction_pixels(
                actual=self.background, background=self.background,
                expected_direction="north", sprites=self.sprites,
            )


if __name__ == "__main__":
    unittest.main()
