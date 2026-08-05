#!/usr/bin/env python3
"""Convert original pointer-based fog edge DATs into pointer-free spans."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

SHAPES = 17
CLASSES = 47


def payload(data: bytes, offset: int) -> bytes:
    result = bytearray()
    while offset < len(data) and data[offset] != 0xFF:
        if offset + 3 > len(data):
            raise ValueError("truncated fog edge payload")
        result.extend(data[offset:offset + 3])
        offset += 3
    if offset >= len(data):
        raise ValueError("unterminated fog edge payload")
    result.append(0xFF)
    return bytes(result)


def tables(path: Path, pointers_per_class: int) -> list[bytes]:
    data = path.read_bytes()
    shapes = struct.unpack_from("<17I", data)
    result: list[bytes] = []
    for shape in shapes:
        for edge_class in range(CLASSES):
            for side in range(pointers_per_class):
                slot = shape + (edge_class * pointers_per_class + side) * 4
                pointer = struct.unpack_from("<I", data, slot)[0]
                result.append(payload(data, pointer) if pointer else b"\xff")
    return result


def emit(tile: list[bytes], black: list[bytes], output: Path) -> None:
    unique: dict[bytes, int] = {}
    blob = bytearray()

    def offsets(entries: list[bytes]) -> list[int]:
        result = []
        for entry in entries:
            if entry not in unique:
                unique[entry] = len(blob)
                blob.extend(entry)
            result.append(unique[entry])
        return result

    tile_offsets = offsets(tile)
    black_offsets = offsets(black)

    def array(
        name: str,
        values: list[int],
        width: int = 12,
        value_type: str = "std::uint32_t",
    ) -> str:
        lines = []
        for index in range(0, len(values), width):
            lines.append("    " + ", ".join(map(str, values[index:index + width])))
        return (
            f"inline constexpr std::array<{value_type}, {len(values)}> {name}{{{{\n"
            + ",\n".join(lines) + "\n}};\n"
        )

    byte_values = list(blob)
    text = (
        "// Generated pointer-free geometry. Source hashes documented in "
        "FOG_RENDERING_FIDELITY.md.\n#pragma once\n\n#include <array>\n"
        "#include <cstdint>\n\nnamespace aoe::fog::generated {\n\n"
        + array("tile_offsets", tile_offsets)
        + "\n" + array("black_offsets", black_offsets)
        + "\n" + array("span_bytes", byte_values, 24, "std::uint8_t")
        + "\n}  // namespace aoe::fog::generated\n"
    )
    output.write_text(text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tile-edge", type=Path, required=True)
    parser.add_argument("--black-edge", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    emit(tables(args.tile_edge, 2), tables(args.black_edge, 1), args.output)


if __name__ == "__main__":
    main()
