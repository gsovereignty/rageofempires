#!/usr/bin/env python3

import json
import struct
import tempfile
import unittest
from pathlib import Path

from PIL import Image

from nostr_packaged_pixel_oracle import (
    evaluate_packaged_capture,
    render_decoded_draw,
    write_wrong_direction_mutation,
)
from nostr_slp_decoder import decode_slp_frame


def synthetic_slp() -> bytes:
    command = bytes((0x02, 128, *range(1, 129), 0x0F))
    payload = bytearray(84 + len(command))
    payload[:4] = b"2.0N"
    struct.pack_into("<i", payload, 4, 1)
    struct.pack_into("<II", payload, 32, 80, 76)
    struct.pack_into("<iiii", payload, 48, 128, 1, 32, 0)
    struct.pack_into("<HH", payload, 76, 0, 0)
    struct.pack_into("<I", payload, 80, 84)
    payload.extend(command)
    return bytes(payload)


def synthetic_drs(extension: str, identifier: int, resource: bytes) -> bytes:
    data = bytearray(88 + len(resource))
    data[40:44] = b"1.00"
    struct.pack_into("<i", data, 56, 1)
    data[64:68] = extension.encode()[::-1].ljust(4, b" ")
    struct.pack_into("<ii", data, 68, 76, 1)
    struct.pack_into("<iii", data, 76, identifier, 88, len(resource))
    data[88:] = resource
    return bytes(data)


class PackagedPixelOracleTests(unittest.TestCase):
    def test_real_capture_pipeline_passes_and_flip_mutation_fails(self):
        palette_payload = (
            "JASC-PAL\n0100\n256\n" +
            "".join(f"{index} {index // 2} {255 - index}\n"
                    for index in range(256))
        ).encode()
        palette = [
            (index, index // 2, 255 - index) for index in range(256)
        ]
        slp = synthetic_slp()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            graphics = root / "graphics.drs"
            interface = root / "interfac.drs"
            graphics.write_bytes(synthetic_drs("slp", 1484, slp))
            interface.write_bytes(
                synthetic_drs("bina", 50500, palette_payload)
            )
            background = Image.new("RGBA", (170, 12), (30, 70, 20, 255))
            sprite = render_decoded_draw(
                canvas_size=background.size, payload=slp, palette=palette,
                frame_index=0, legacy_player=0, ground=(85, 6), zoom=1,
                flip_horizontal=False, visible=True,
            )
            actual = background.copy()
            actual.alpha_composite(sprite)
            actual.save(root / "host-gameplay.png")
            background.save(root / "host-terrain.png")
            decoded = decode_slp_frame(slp, palette, 0, 0).image
            decoded.save(root / "host-unit-7-sprite.png")
            manifest = {"cases": [{
                "actual": "host-gameplay.png",
                "terrain": "host-terrain.png",
                "sprite": "host-unit-7-sprite.png",
                "x": 2, "y": 6,
                "metadata": {
                    "entity_id": 7, "zoom": 1, "tick": 12,
                    "sprite_frames": [{
                        "resource_id": 1484, "frame": 0,
                        "palette_player": 0, "flip_horizontal": False,
                        "visible": True, "ground": [85, 6],
                        "destination": [53, 6, 128, 1],
                        "clipped_destination": [53, 6, 117, 1],
                        "action_frame": 0, "frames_per_direction": 1,
                        "direction_count": 2, "mirroring_mode": 1,
                        "physical_frame_count": 1,
                        "logical_direction": 0,
                    }],
                },
            }]}
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest))
            passed = evaluate_packaged_capture(
                manifest_path=manifest_path, graphics_drs=graphics,
                interface_drs=interface, expected_logical_direction=0,
                evidence_directory=root / "pass",
            )
            self.assertEqual(passed["verdict"], "PASS")
            self.assertEqual(passed["bestDirection"], "0")

            wrong_sprite = render_decoded_draw(
                canvas_size=background.size, payload=slp, palette=palette,
                frame_index=0, legacy_player=0, ground=(85, 6), zoom=1,
                flip_horizontal=True, visible=True,
            )
            wrong = background.copy()
            wrong.alpha_composite(wrong_sprite)
            wrong.save(root / "host-gameplay.png")
            failed = evaluate_packaged_capture(
                manifest_path=manifest_path, graphics_drs=graphics,
                interface_drs=interface, expected_logical_direction=0,
                evidence_directory=root / "fail",
            )
            self.assertEqual(failed["verdict"], "FAIL")
            self.assertEqual(failed["bestDirection"], "1")

    def test_retained_wrong_direction_mutation_fails(self):
        palette_payload = (
            "JASC-PAL\n0100\n256\n" +
            "".join(f"{index} {index // 2} {255 - index}\n"
                    for index in range(256))
        ).encode()
        palette = [(index, index // 2, 255 - index)
                   for index in range(256)]
        slp = synthetic_slp()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            graphics = root / "graphics.drs"
            interface = root / "interfac.drs"
            graphics.write_bytes(synthetic_drs("slp", 1484, slp))
            interface.write_bytes(synthetic_drs("bina", 50500, palette_payload))
            background = Image.new("RGBA", (170, 12), (30, 70, 20, 255))
            sprite = render_decoded_draw(
                canvas_size=background.size, payload=slp, palette=palette,
                frame_index=0, legacy_player=0, ground=(85, 6), zoom=1,
                flip_horizontal=False, visible=True,
            )
            actual = background.copy()
            actual.alpha_composite(sprite)
            actual.save(root / "actual.png")
            background.save(root / "terrain.png")
            manifest = {"cases": [{
                "actual": "actual.png", "terrain": "terrain.png",
                "metadata": {"entity_id": 7, "zoom": 1, "tick": 12,
                    "sprite_frames": [{"resource_id": 1484, "frame": 0,
                        "palette_player": 0, "flip_horizontal": False,
                        "visible": True, "ground": [85, 6],
                        "action_frame": 0, "frames_per_direction": 1,
                        "direction_count": 2, "mirroring_mode": 1,
                        "physical_frame_count": 1,
                        "logical_direction": 0}]}}]}
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps(manifest))
            report = write_wrong_direction_mutation(
                manifest_path=manifest_path, graphics_drs=graphics,
                interface_drs=interface, expected_logical_direction=0,
                evidence_directory=root / "mutation",
            )
            self.assertEqual(report["verdict"], "FAIL")
            self.assertEqual(report["metadataLogicalDirection"], 0)
            self.assertTrue((root / "mutation/mutation.json").is_file())

    def test_composite_layers_pass_and_one_layer_mutation_fails(self):
        palette_payload = (
            "JASC-PAL\n0100\n256\n" +
            "".join(f"{index} {index // 2} {255 - index}\n"
                    for index in range(256))
        ).encode()
        palette = [(index, index // 2, 255 - index)
                   for index in range(256)]
        slp = synthetic_slp()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            graphics = root / "graphics.drs"
            interface = root / "interfac.drs"
            graphics.write_bytes(synthetic_drs("slp", 1484, slp))
            interface.write_bytes(synthetic_drs("bina", 50500, palette_payload))
            background = Image.new("RGBA", (240, 12), (30, 70, 20, 255))
            actual = background.copy()
            draws = []
            for ground_x in (70, 170):
                actual.alpha_composite(render_decoded_draw(
                    canvas_size=background.size, payload=slp,
                    palette=palette, frame_index=0, legacy_player=0,
                    ground=(ground_x, 6), zoom=1, flip_horizontal=False,
                    visible=True,
                ))
                draws.append({
                    "resource_id": 1484, "frame": 0,
                    "palette_player": 0, "flip_horizontal": False,
                    "visible": True, "ground": [ground_x, 6],
                    "action_frame": 0, "frames_per_direction": 1,
                    "direction_count": 2, "mirroring_mode": 1,
                    "physical_frame_count": 1, "logical_direction": 0,
                })
            actual.save(root / "actual.png")
            background.save(root / "terrain.png")
            manifest_path = root / "manifest.json"
            manifest_path.write_text(json.dumps({"cases": [{
                "actual": "actual.png", "terrain": "terrain.png",
                "metadata": {"entity_id": 7, "zoom": 1, "tick": 12,
                             "sprite_frames": draws},
            }]}))
            passed = evaluate_packaged_capture(
                manifest_path=manifest_path, graphics_drs=graphics,
                interface_drs=interface, expected_logical_direction=0,
                evidence_directory=root / "pass",
            )
            self.assertEqual(passed["verdict"], "PASS")
            self.assertEqual(len(passed["actualLayers"]), 2)
            mutation = write_wrong_direction_mutation(
                manifest_path=manifest_path, graphics_drs=graphics,
                interface_drs=interface, expected_logical_direction=0,
                evidence_directory=root / "mutation",
            )
            self.assertEqual(mutation["verdict"], "FAIL")
            self.assertEqual(mutation["mutatedLayer"], 0)


if __name__ == "__main__":
    unittest.main()
