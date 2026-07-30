import importlib.util
import json
import struct
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("generate_audio_catalog.py")
SPEC = importlib.util.spec_from_file_location("generate_audio_catalog", MODULE_PATH)
CATALOG = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CATALOG)


def wav(payload):
    fmt = struct.pack("<HHIIHH", 1, 1, 8000, 8000, 1, 8)
    chunks = b"fmt " + struct.pack("<I", len(fmt)) + fmt
    chunks += b"data" + struct.pack("<I", len(payload)) + payload
    return b"RIFF" + struct.pack("<I", len(chunks) + 4) + b"WAVE" + chunks


def drs(entries):
    table_info = 76
    payload_offset = table_info + len(entries) * 12
    header = bytearray(64)
    header[40:42] = b"1."
    struct.pack_into("<i", header, 56, 1)
    table = b"vaw " + struct.pack("<ii", table_info, len(entries))
    directory = bytearray()
    payloads = bytearray()
    for resource_id, payload in entries:
        directory += struct.pack(
            "<iii", resource_id, payload_offset + len(payloads), len(payload)
        )
        payloads += payload
    return bytes(header) + table + bytes(directory) + bytes(payloads)


class AudioCatalogTests(unittest.TestCase):
    def test_exact_join_and_later_archive_precedence(self):
        metadata = {
            "source": {"path": "test.dat"},
            "sounds": [
                {
                    "id": 0,
                    "play_delay": 0,
                    "items": [
                        {
                            "civilization": -1,
                            "filename": "",
                            "icon_set": -1,
                            "probability": 100,
                            "resource_id": 10,
                        },
                    ],
                },
                {
                    "id": 1,
                    "play_delay": 0,
                    "items": [
                        {
                            "civilization": -1,
                            "filename": "",
                            "icon_set": -1,
                            "probability": 100,
                            "resource_id": 99,
                        },
                    ],
                },
                {
                    "id": 2,
                    "play_delay": 0,
                    "items": [
                        {
                            "civilization": -1,
                            "filename": "",
                            "icon_set": -1,
                            "probability": 100,
                            "resource_id": -1,
                        },
                    ],
                },
            ],
        }
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / "sounds.drs"
            x1 = root / "sounds_x1.drs"
            base.write_bytes(drs([(10, wav(b"a")), (11, wav(b"orphan"))]))
            x1.write_bytes(drs([(10, wav(b"b"))]))
            result = CATALOG.make_catalog(
                metadata, [("sounds.drs", base), ("sounds_x1.drs", x1)]
            )

        self.assertEqual(result["summary"]["duplicate_resource_id_count"], 1)
        self.assertEqual(result["summary"]["byte_identical_duplicate_count"], 0)
        self.assertEqual(result["missing_referenced_resource_ids"], [99])
        self.assertEqual(result["unreferenced_archive_resource_ids"], [11])
        item = result["sounds"][0]["items"][0]
        self.assertEqual(item["resolved_archive"], "sounds_x1.drs")
        self.assertEqual(item["wav_format"]["sample_rate"], 8000)
        self.assertEqual(item["classification"], "exact")
        sentinel = result["sounds"][2]["items"][0]
        self.assertEqual(sentinel["unavailable_reason"], "negative DAT sentinel")

    def test_rejects_payload_outside_archive(self):
        broken = bytearray(drs([(10, wav(b"a"))]))
        struct.pack_into("<i", broken, 76 + 4, len(broken) + 1)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "broken.drs"
            path.write_bytes(broken)
            with self.assertRaisesRegex(ValueError, "payload outside"):
                CATALOG.read_drs_wavs(path)


class CheckedInAudioCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads(Path("generated/audio_catalog.json").read_text())

    def test_exhaustive_live_join_counts(self):
        self.assertEqual(self.data["schema"], "aoe-audio-catalog-v1")
        self.assertEqual(
            self.data["summary"],
            {
                "dat_sound_count": 506,
                "dat_nonempty_sound_count": 493,
                "dat_sound_item_count": 1730,
                "referenced_unique_wav_count": 1650,
                "available_referenced_unique_wav_count": 1314,
                "missing_referenced_unique_wav_count": 336,
                "resolved_archive_unique_wav_count": 1328,
                "unreferenced_archive_wav_count": 14,
                "duplicate_resource_id_count": 58,
                "byte_identical_duplicate_count": 58,
            },
        )
        self.assertEqual(len(self.data["sounds"]), 506)

    def test_every_available_dat_item_is_a_valid_riff_wave(self):
        available = [
            item
            for sound in self.data["sounds"]
            for item in sound["items"]
            if item["available"]
        ]
        self.assertEqual(len(available), 1371)
        self.assertTrue(all(x["wav_format"]["riff_wave"] for x in available))

    def test_live_archive_identity_and_precedence_are_pinned(self):
        archives = {x["name"]: x for x in self.data["archives"]}
        self.assertEqual(
            archives["sounds.drs"]["sha256"],
            "292eeff24657b70c9fa277a55daa0e6a47bc245141c0c533bab54eecda244fa6",
        )
        self.assertEqual(
            archives["sounds_x1.drs"]["sha256"],
            "48efbb286fb73d4aa88ed7c05ab38cb0d045323ae04aed13f21792034b30f6c5",
        )
        self.assertEqual(
            self.data["lookup_contract"]["precedence"],
            ["sounds.drs", "sounds_x1.drs"],
        )


if __name__ == "__main__":
    unittest.main()
