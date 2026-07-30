import tempfile
import unittest
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_content_assets import inventory


class ContentAssetAuditTests(unittest.TestCase):
    def test_classifies_and_hashes_supported_content(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixtures = {
                "Campaign/one.AOE2CAMPAIGN": b"campaign",
                "Scenario/two.scx": b"scenario",
                "resources/language_x1_p1.dll": b"language",
                "Sound/stream/town.MP3": b"music",
                "Data/sounds_x1.drs": b"archive",
                "AoK HD.exe": b"ignored",
            }
            for name, content in fixtures.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(content)

            report = inventory(root)
            categories = report["categories"]
            self.assertEqual(categories["campaign"]["count"], 1)
            self.assertEqual(categories["scenario"]["count"], 1)
            self.assertEqual(categories["localization"]["count"], 1)
            self.assertEqual(categories["audio"]["count"], 1)
            self.assertEqual(categories["audio_archive"]["count"], 1)
            self.assertEqual(
                categories["campaign"]["files"][0]["path"],
                "Campaign/one.AOE2CAMPAIGN",
            )
            self.assertEqual(
                len(categories["campaign"]["files"][0]["sha256"]), 64
            )

    def test_output_is_deterministic_and_skips_symlinks(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "b.scn").write_bytes(b"b")
            (root / "a.scn").write_bytes(b"a")
            (root / "linked.scn").symlink_to(root / "a.scn")

            first = inventory(root)
            second = inventory(root)
            self.assertEqual(first, second)
            self.assertEqual(
                [
                    item["path"]
                    for item in first["categories"]["scenario"]["files"]
                ],
                ["a.scn", "b.scn"],
            )

    def test_rejects_non_directory_root(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "missing"
            with self.assertRaisesRegex(ValueError, "not a directory"):
                inventory(path)


if __name__ == "__main__":
    unittest.main()
