#!/usr/bin/env python3

import struct
import tempfile
import unittest
from pathlib import Path

from nostr_slp_decoder import (
    SlpDecodeError,
    decode_slp_frame,
    drs_resource,
    parse_jasc_palette,
)


def synthetic_slp(command: bytes, width: int = 4) -> bytes:
    payload = bytearray(84 + len(command))
    payload[:4] = b"2.0N"
    struct.pack_into("<i", payload, 4, 1)
    struct.pack_into("<II", payload, 32, 80, 76)
    struct.pack_into("<iiii", payload, 48, width, 1, 2, 3)
    struct.pack_into("<HH", payload, 76, 0, 0)
    struct.pack_into("<I", payload, 80, 84)
    payload.extend(command)
    return bytes(payload)


def synthetic_drs(resource: bytes) -> bytes:
    data = bytearray(88 + len(resource))
    data[40:44] = b"1.00"
    struct.pack_into("<i", data, 56, 1)
    data[64:68] = b"pls "
    struct.pack_into("<ii", data, 68, 76, 1)
    struct.pack_into("<iii", data, 76, 1479, 88, len(resource))
    data[88:] = resource
    return bytes(data)


class IndependentSlpDecoderTests(unittest.TestCase):
    def setUp(self):
        self.palette = [(index, 0, 255 - index) for index in range(256)]

    def test_literal_skip_fill_and_player_commands(self):
        cases = (
            (bytes((0x08, 1, 2, 0x05, 0x04, 3, 0x0F)),
             [(1, 0, 254, 255), (2, 0, 253, 255),
              (0, 0, 0, 0), (3, 0, 252, 255)]),
            (bytes((0x26, 1, 2, 0x2A, 3, 0x0F)),
             [(17, 0, 238, 255), (18, 0, 237, 255),
              (19, 0, 236, 255), (19, 0, 236, 255)]),
        )
        for command, expected in cases:
            with self.subTest(command=command):
                decoded = decode_slp_frame(
                    synthetic_slp(command), self.palette, 0, 16
                )
                self.assertEqual(decoded.image.size, (4, 1))
                self.assertEqual(list(decoded.image.getdata()), expected)
                self.assertEqual((decoded.hotspot_x, decoded.hotspot_y), (2, 3))

    def test_outline_commands_leave_transparent_pixels(self):
        decoded = decode_slp_frame(
            synthetic_slp(bytes((0x4E, 0x3B, 0x0F))),
            self.palette, 0, 16,
        )
        self.assertEqual(list(decoded.image.getdata()),
                         [(0, 0, 0, 0)] * 4)

    def test_reads_resource_from_classic_drs(self):
        resource = synthetic_slp(bytes((0x10, 1, 2, 3, 4, 0x0F)))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "graphics.drs"
            path.write_bytes(synthetic_drs(resource))
            self.assertEqual(drs_resource(path, "slp", 1479), resource)

    def test_parses_jasc_palette(self):
        payload = b"JASC-PAL\n0100\n2\n1 2 3\n4 5 6\n"
        self.assertEqual(parse_jasc_palette(payload), [(1, 2, 3), (4, 5, 6)])

    def test_malformed_inputs_fail_closed(self):
        for payload in (b"", synthetic_slp(bytes((0x04, 1, 0x0F)))[0:82]):
            with self.subTest(size=len(payload)), self.assertRaises(SlpDecodeError):
                decode_slp_frame(payload, self.palette, 0, 16)


if __name__ == "__main__":
    unittest.main()
