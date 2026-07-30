#!/usr/bin/env python3
"""Reject regression capture containing old opaque procedural blue body."""

import argparse
import struct
from pathlib import Path


def blue_body_pixels(path: Path) -> int:
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise ValueError("capture is not BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    bits = struct.unpack_from("<H", data, 28)[0]
    if bits not in (24, 32):
        raise ValueError("capture BMP must use 24-bit or 32-bit RGB")
    channels = bits // 8
    stride = ((abs(width) * channels + 3) // 4) * 4
    target = bytes((150, 75, 35))  # BMP stores BGR for RGB 35,75,150.
    count = 0
    for row in range(abs(height)):
        start = offset + row * stride
        for column in range(abs(width)):
            pixel = start + column * channels
            count += data[pixel:pixel + 3] == target
    return count


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--maximum", type=int, default=500)
    args = parser.parse_args()
    count = blue_body_pixels(args.capture)
    if count > args.maximum:
        raise SystemExit(
            f"opaque procedural blue body regression: {count} pixels"
        )
    print(f"opaque procedural blue pixels: {count}")


if __name__ == "__main__":
    main()
