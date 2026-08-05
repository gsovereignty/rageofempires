#!/usr/bin/env python3

import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).with_name("generate_fog_edge_geometry.py")
SPEC = importlib.util.spec_from_file_location("fog_geometry", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(MODULE)


def fixture(pointers_per_class: int) -> bytes:
    table_size = MODULE.CLASSES * pointers_per_class * 4
    offsets = [MODULE.SHAPES * 4 + index * table_size
               for index in range(MODULE.SHAPES)]
    data = bytearray(struct.pack("<17I", *offsets))
    data.extend(b"\0" * (MODULE.SHAPES * table_size))
    payload = len(data)
    data.extend(bytes((0, 48, 48, 1, 46, 50, 0xFF)))
    struct.pack_into("<I", data, offsets[0], payload)
    return bytes(data)


class FogGeometryGeneratorTests(unittest.TestCase):
    def test_pointer_tables_become_local_terminated_spans(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            tile = root / "TileEdge.Dat"
            black = root / "BlkEdge.Dat"
            output = root / "fog_edge_geometry.hpp"
            tile.write_bytes(fixture(2))
            black.write_bytes(fixture(1))
            tile_entries = MODULE.tables(tile, 2)
            black_entries = MODULE.tables(black, 1)
            self.assertEqual(tile_entries[0], bytes((0, 48, 48, 1, 46, 50, 0xFF)))
            self.assertEqual(tile_entries[1], b"\xff")
            self.assertEqual(len(tile_entries), 17 * 47 * 2)
            self.assertEqual(len(black_entries), 17 * 47)
            MODULE.emit(tile_entries, black_entries, output)
            generated = output.read_text()
            self.assertIn("tile_offsets", generated)
            self.assertIn("black_offsets", generated)
            self.assertNotIn(str(tile), generated)


if __name__ == "__main__":
    unittest.main()
