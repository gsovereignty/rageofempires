#!/usr/bin/env python3
"""Launch deterministic headless render and validate its broad visual structure."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import struct
import subprocess
import tempfile


EXPECTED_WIDTH = 800
EXPECTED_HEIGHT = 450


def load_bmp(path: Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    data = path.read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError("capture is not a BMP file")

    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    dib_size = struct.unpack_from("<I", data, 14)[0]
    width, signed_height = struct.unpack_from("<ii", data, 18)
    planes, bits_per_pixel = struct.unpack_from("<HH", data, 26)
    compression = struct.unpack_from("<I", data, 30)[0]
    if dib_size < 40 or planes != 1 or bits_per_pixel not in (24, 32):
        raise ValueError(
            f"unsupported BMP layout: DIB={dib_size}, planes={planes}, "
            f"bits={bits_per_pixel}"
        )
    if compression != 0 or width <= 0 or signed_height == 0:
        raise ValueError("capture must be an uncompressed, nonempty BMP")

    height = abs(signed_height)
    bytes_per_pixel = bits_per_pixel // 8
    stride = ((width * bits_per_pixel + 31) // 32) * 4
    required_size = pixel_offset + stride * height
    if required_size > len(data):
        raise ValueError("capture pixel data is truncated")

    pixels: list[tuple[int, int, int]] = []
    for output_y in range(height):
        source_y = height - 1 - output_y if signed_height > 0 else output_y
        row = pixel_offset + source_y * stride
        for x in range(width):
            offset = row + x * bytes_per_pixel
            blue, green, red = data[offset : offset + 3]
            pixels.append((red, green, blue))
    return width, height, pixels


def region(
    pixels: list[tuple[int, int, int]],
    width: int,
    left: int,
    top: int,
    right: int,
    bottom: int,
) -> list[tuple[int, int, int]]:
    return [
        pixels[y * width + x]
        for y in range(top, bottom)
        for x in range(left, right)
    ]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def validate_capture(path: Path) -> None:
    width, height, pixels = load_bmp(path)
    require(
        (width, height) == (EXPECTED_WIDTH, EXPECTED_HEIGHT),
        f"wrong capture dimensions: {width}x{height}, expected "
        f"{EXPECTED_WIDTH}x{EXPECTED_HEIGHT}",
    )

    unique_colors = set(pixels)
    luminance = [(red * 3 + green * 6 + blue) // 10 for red, green, blue in pixels]
    require(len(unique_colors) >= 30, "capture has too little color diversity")
    require(max(luminance) - min(luminance) >= 150, "capture lacks contrast")

    world = region(pixels, width, 0, 0, width, 400)
    hud = region(pixels, width, 0, 401, width, height)
    minimap = region(pixels, width, 660, 401, width, height)

    green_terrain = sum(
        green >= red + 18 and green >= blue + 18 and green >= 55
        for red, green, blue in world
    )
    blue_water = sum(
        blue >= red + 25 and blue >= green + 5 and blue >= 75
        for red, green, blue in world
    )
    bright_hud = sum(max(pixel) >= 170 for pixel in hud)
    warm_border = sum(
        red >= 80 and red >= green and green >= blue * 1.25
        for red, green, blue in hud
    )

    require(green_terrain >= 20_000, "world lacks expected grass terrain")
    require(blue_water >= 300, "world lacks expected water detail")
    require(len(set(world)) >= 20, "world region appears blank or incomplete")
    require(len(set(hud)) >= 10, "HUD region appears blank or incomplete")
    require(bright_hud >= 150, "HUD text/details are missing")
    require(warm_border >= 500, "HUD stone/gold frame is missing")
    # A 255x255 map is compressed into a 214x80 HUD panel, so the minimap
    # holds far fewer distinct colours than a 24x16 map did. Assert the
    # terrain actually reaches it instead of counting colours.
    minimap_green = sum(
        green >= red + 18 and green >= blue + 18 and green >= 55
        for red, green, blue in minimap
    )
    minimap_water = sum(
        blue >= red + 25 and blue >= green + 5 and blue >= 75
        for red, green, blue in minimap
    )
    require(len(set(minimap)) >= 6, "minimap region appears blank")
    require(minimap_green >= 400, "minimap lacks land coverage")
    require(minimap_water >= 20, "minimap lacks water coverage")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", type=Path)
    parser.add_argument("scenario", type=Path)
    parser.add_argument(
        "--camera-tile",
        default="122,127",
        help=(
            "tile to centre the capture on. The startup map is 255x255, so "
            "the default start view holds no water; this frames the river "
            "ford, which has both grass and water."
        ),
    )
    args = parser.parse_args()

    executable = args.executable.resolve()
    scenario = args.scenario.resolve()
    if not executable.is_file():
        parser.error(f"executable does not exist: {executable}")
    if not scenario.is_file():
        parser.error(f"scenario does not exist: {scenario}")

    with tempfile.TemporaryDirectory(prefix="aoe-visual-smoke-") as directory:
        capture = Path(directory) / "frame.bmp"
        dimensions = Path(directory) / "dimensions.txt"
        environment = os.environ.copy()
        environment.update(
            {
                "SDL_VIDEO_DRIVER": "dummy",
                "AOE_WINDOW_SIZE": "800x600",
                "AOE_SCENARIO_PATH": str(scenario),
                "AOE_CAMERA_TILE": args.camera_tile,
                # The map is larger than one start's explored area, so the
                # capture would otherwise be mostly fog.
                "AOE_FOG": "0",
                "AOE_MAP_DIMENSION_PATH": str(dimensions),
                "AOE_SCREENSHOT_PATH": str(capture),
                "AOE_SCREENSHOT_TICK": "0",
                "AOE_EXIT_AFTER_SCREENSHOT": "1",
            }
        )
        completed = subprocess.run(
            [str(executable)],
            env=environment,
            capture_output=True,
            text=True,
            timeout=90,
            check=False,
        )
        if completed.returncode != 0:
            detail = (completed.stderr or completed.stdout).strip()
            raise RuntimeError(
                f"render process exited {completed.returncode}: {detail[:800]}"
            )
        if not capture.is_file() or capture.stat().st_size == 0:
            raise RuntimeError("render process produced no screenshot")
        require(
            dimensions.read_text() == "map 255 255\ntiles 65025\n",
            "runtime did not present the maximum 255x255 map",
        )
        validate_capture(capture)

    print("visual smoke passed: world, HUD, minimap, terrain, and water present")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
