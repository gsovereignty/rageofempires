#!/usr/bin/env python3

import hashlib
import json
import unittest
from pathlib import Path


class Mp3EffectPackTests(unittest.TestCase):
    def test_pack_covers_every_available_catalog_resource(self) -> None:
        root = Path(__file__).resolve().parents[1]
        catalog = json.loads((root / "generated/audio_catalog.json").read_text())
        expected = {
            int(item["resource_id"])
            for sound in catalog["sounds"]
            for item in sound["items"]
            if item["available"]
        }
        directory = root / "game_data/Sound/effects"
        manifest = json.loads((directory / "manifest.json").read_text())
        actual = {int(item["resource_id"]) for item in manifest["effects"]}
        self.assertEqual(actual, expected)
        self.assertEqual(int(manifest["effect_count"]), len(expected))
        for item in manifest["effects"]:
            path = directory / f"{item['resource_id']}.mp3"
            payload = path.read_bytes()
            self.assertGreater(len(payload), 0)
            self.assertEqual(len(payload), int(item["byte_size"]))
            self.assertEqual(hashlib.sha256(payload).hexdigest(), item["sha256"])


if __name__ == "__main__":
    unittest.main()
