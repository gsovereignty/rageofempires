#!/usr/bin/env python3
"""Independent classic DRS/SLP decoder used by semantic pixel oracle."""

from __future__ import annotations

import hashlib
import struct
from dataclasses import dataclass
from pathlib import Path

from PIL import Image


DECODER_VERSION = "classic-slp-2.0-v1"


class SlpDecodeError(ValueError):
    """Packaged asset does not satisfy supported classic DRS/SLP contract."""


@dataclass(frozen=True)
class DecodedSlpFrame:
    image: Image.Image
    hotspot_x: int
    hotspot_y: int
    payload_sha256: str
    decoder_version: str = DECODER_VERSION


def drs_resource(path: Path, extension: str, resource_id: int) -> bytes:
    data = path.read_bytes()
    if len(data) < 64 or data[40:42] != b"1.":
        raise SlpDecodeError("invalid classic DRS header")
    table_count = struct.unpack_from("<i", data, 56)[0]
    if table_count < 0 or 64 + table_count * 12 > len(data):
        raise SlpDecodeError("DRS table directory outside archive")
    wanted = extension.lower().strip()
    for table in range(table_count):
        offset = 64 + table * 12
        kind = data[offset:offset + 4][::-1].decode("ascii").strip("\0 ").lower()
        entries, count = struct.unpack_from("<ii", data, offset + 4)
        if kind != wanted:
            continue
        if count < 0 or entries < 0 or entries + count * 12 > len(data):
            raise SlpDecodeError("DRS resource table outside archive")
        for index in range(count):
            record = entries + index * 12
            identifier, payload, size = struct.unpack_from("<iii", data, record)
            if identifier != resource_id:
                continue
            if payload < 0 or size < 0 or payload + size > len(data):
                raise SlpDecodeError("DRS resource outside archive")
            return data[payload:payload + size]
    raise SlpDecodeError(
        f"DRS resource absent: {wanted} {resource_id}"
    )


def parse_jasc_palette(payload: bytes) -> list[tuple[int, int, int]]:
    try:
        lines = payload.decode("ascii").splitlines()
        if lines[:2] != ["JASC-PAL", "0100"]:
            raise SlpDecodeError("invalid JASC palette header")
        count = int(lines[2])
        colors = [tuple(map(int, line.split()))
                  for line in lines[3:3 + count]]
    except (UnicodeDecodeError, IndexError, ValueError) as error:
        raise SlpDecodeError("invalid JASC palette") from error
    if len(colors) != count or any(
        len(color) != 3 or any(channel < 0 or channel > 255 for channel in color)
        for color in colors
    ):
        raise SlpDecodeError("invalid JASC palette colors")
    return colors


def decode_slp_frame(
    payload: bytes,
    palette: list[tuple[int, int, int]],
    frame_index: int,
    player_palette_base: int,
) -> DecodedSlpFrame:
    if len(payload) < 32 or not payload.startswith(b"2.0"):
        raise SlpDecodeError("invalid classic SLP header")
    frame_count = struct.unpack_from("<i", payload, 4)[0]
    if frame_index < 0 or frame_index >= frame_count:
        raise SlpDecodeError("SLP frame index out of range")
    frame_offset = 32 + frame_index * 32
    if frame_offset + 32 > len(payload):
        raise SlpDecodeError("SLP frame table outside payload")
    commands, outline = struct.unpack_from("<II", payload, frame_offset)
    width, height, hotspot_x, hotspot_y = struct.unpack_from(
        "<iiii", payload, frame_offset + 16
    )
    if width <= 0 or height <= 0 or width * height > 64_000_000:
        raise SlpDecodeError("invalid SLP frame dimensions")
    if commands + height * 4 > len(payload) or outline + height * 4 > len(payload):
        raise SlpDecodeError("SLP row tables outside payload")
    rgba = bytearray(width * height * 4)

    def put(row: int, column: int, palette_index: int) -> None:
        if column < 0 or column >= width or palette_index >= len(palette):
            raise SlpDecodeError("SLP pixel outside frame or palette")
        offset = (row * width + column) * 4
        rgba[offset:offset + 4] = bytes((*palette[palette_index], 255))

    for row in range(height):
        left, right = struct.unpack_from("<HH", payload, outline + row * 4)
        if left == 0x8000 or right == 0x8000:
            continue
        if left + right > width:
            raise SlpDecodeError("invalid SLP row outline")
        position = struct.unpack_from("<I", payload, commands + row * 4)[0]
        column = left
        row_end = width - right

        def take() -> int:
            nonlocal position
            if position >= len(payload):
                raise SlpDecodeError("truncated SLP command stream")
            value = payload[position]
            position += 1
            return value

        def count(command: int, shift: int) -> int:
            amount = command >> shift
            return take() if amount == 0 else amount

        while True:
            command = take()
            low = command & 0x0F
            high = command & 0xF0
            crumb = command & 0x03
            if low == 0x0F:
                break
            if crumb == 0:
                amount = command >> 2
                for _ in range(amount):
                    put(row, column, take())
                    column += 1
            elif crumb == 1:
                column += count(command, 2)
            elif low in (0x02, 0x03):
                amount = (high << 4) + take()
                if low == 0x02:
                    for _ in range(amount):
                        put(row, column, take())
                        column += 1
                else:
                    column += amount
            elif low == 0x06:
                amount = count(command, 4)
                for _ in range(amount):
                    put(row, column, player_palette_base + take())
                    column += 1
            elif low in (0x07, 0x0A):
                amount = count(command, 4)
                palette_index = take()
                if low == 0x0A:
                    palette_index += player_palette_base
                for _ in range(amount):
                    put(row, column, palette_index)
                    column += 1
            elif low == 0x0B:
                column += count(command, 4)
            elif low == 0x0E and high in (0x40, 0x60):
                column += 1
            elif low == 0x0E and high in (0x50, 0x70):
                column += take()
            else:
                raise SlpDecodeError(
                    f"unsupported SLP command 0x{command:02x}"
                )
            if column > row_end:
                raise SlpDecodeError("SLP command crosses row outline")
        if column != row_end:
            raise SlpDecodeError("SLP row pixel count mismatch")
    return DecodedSlpFrame(
        Image.frombytes("RGBA", (width, height), bytes(rgba)),
        hotspot_x,
        hotspot_y,
        hashlib.sha256(payload).hexdigest(),
    )
