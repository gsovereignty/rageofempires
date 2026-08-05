#!/usr/bin/env python3

import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("audit_fog_contract.py")
SPEC = importlib.util.spec_from_file_location("audit_fog_contract", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


def edge_file(pointer_width: int) -> bytes:
    table_bytes = MODULE.EDGE_CLASSES * pointer_width
    offsets = [
        MODULE.SHAPES * 4 + shape * table_bytes
        for shape in range(MODULE.SHAPES)
    ]
    data = bytearray(struct.pack("<17I", *offsets))
    data.extend(b"\0" * (MODULE.SHAPES * table_bytes))
    return bytes(data)


def edge_file_with_payload(pointer_width: int) -> bytes:
    data = bytearray(edge_file(pointer_width))
    payload = len(data)
    data.extend(bytes((1, 2, 9, 2, 1, 10, 0xFF)))
    first_table = struct.unpack_from("<I", data)[0]
    struct.pack_into("<I", data, first_table, payload)
    return bytes(data)


class FogContractTests(unittest.TestCase):
    def test_canonical_neighbor_table_has_exact_47_classes(self):
        classes, count = MODULE.canonical_neighbor_classes()
        self.assertEqual(len(classes), 256)
        self.assertEqual(count, 47)
        self.assertEqual(set(classes), set(range(47)))

    def test_edge_table_dimensions_and_bounds(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "BlkEdge.Dat"
            path.write_bytes(edge_file(4))
            result = MODULE.edge_table(path, 4)
            self.assertEqual(result["shape_count"], 17)
            self.assertEqual(result["edge_class_count"], 47)
            self.assertEqual(result["pointers_per_class"], 1)

    def test_invalid_payload_pointer_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "TileEdge.Dat"
            data = bytearray(edge_file(8))
            first_table = struct.unpack_from("<I", data)[0]
            struct.pack_into("<I", data, first_table, len(data) + 1)
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "outside file"):
                MODULE.edge_table(path, 8)

    def test_span_payload_must_terminate_on_record_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "BlkEdge.Dat"
            path.write_bytes(edge_file_with_payload(4))
            result = MODULE.edge_table(path, 4)
            self.assertEqual(result["shapes"][0]["maximum_span_records"], 2)

            data = bytearray(edge_file(4))
            first_table = struct.unpack_from("<I", data)[0]
            struct.pack_into("<I", data, first_table, len(data) - 2)
            data[-2:] = b"\x01\x02"
            path.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "unterminated"):
                MODULE.edge_table(path, 4)

    def test_catalog_promotes_proved_archive_renderer(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exe = root / "game.exe"
            exe.write_bytes(
                b"TileEdge.Dat\0BlkEdge.Dat\0"
                b"diam_map::draw_explored_tiles\0"
            )
            tile = root / "TileEdge.Dat"
            tile.write_bytes(edge_file(8))
            black = root / "BlkEdge.Dat"
            black.write_bytes(edge_file(4))
            interface = root / "interfac.drs"
            interface.write_bytes(b"interface")
            graphics = root / "graphics.drs"
            graphics.write_bytes(b"graphics")
            report = MODULE.make_catalog(
                exe, tile, black, interface, graphics
            )
            self.assertTrue(
                report["renderer_decision"]["archive_backed_world_fog"]
            )
            self.assertIn(
                "compass_bit_order", report["proved"]
            )
            self.assertNotIn("palette_dither", report["unproved"])


if __name__ == "__main__":
    unittest.main()
