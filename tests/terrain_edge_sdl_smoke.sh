#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-terrain-edge.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    name=$1
    zoom=$2
    disabled=$3
    shift 3
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        "AOE_DISABLE_LEGACY_ASSETS=$disabled" \
        AOE_AUDIT_ANY_MAP_SIZE=1 \
        AOE_FOG=0 \
        AOE_WINDOW_SIZE=800x600 \
        AOE_CAMERA_TILE=6,4 \
        "AOE_CAMERA_ZOOM=$zoom" \
        "AOE_SCENARIO_PATH=$script_dir/../resources/water-render-audit.scenario" \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$name.bmp" \
        "$@" \
        "$app_path" >"$smoke_dir/$name.log" 2>&1
}

capture fallback-1.0 1.0 1
capture fallback-1.25 1.25 1
capture fallback-1.5 1.5 1

python3 - "$smoke_dir" <<'PY'
import pathlib
import struct
import sys


def read(path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise SystemExit(f"{path.name}: not BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    stored_height = struct.unpack_from("<i", data, 22)[0]
    height = abs(stored_height)
    bpp = struct.unpack_from("<H", data, 28)[0]
    stride = ((width * bpp + 31) // 32) * 4
    step = bpp // 8

    def pixel(x, y):
        source_y = height - 1 - y if stored_height > 0 else y
        at = offset + source_y * stride + x * step
        blue, green, red = data[at:at + 3]
        return red, green, blue

    return width, height, pixel


root = pathlib.Path(sys.argv[1])
for zoom in ("1.0", "1.25", "1.5"):
    name = f"fallback-{zoom}"
    width, height, pixel = read(root / f"{name}.bmp")
    if (width, height) != (800, 600):
        raise SystemExit(f"{name}: wrong capture dimensions")
    intermediate = []
    maximum_plateau = 0
    for y in range(80, 360):
        previous = None
        run = 0
        for x in range(160, 680):
            color = pixel(x, y)
            blended = (
                69 < color[0] < 92
                and 114 < color[1] < 138
                and 80 < color[2] < 168
            )
            if blended:
                intermediate.append(color)
                run = run + 1 if color == previous else 1
                previous = color
                maximum_plateau = max(maximum_plateau, run)
            else:
                previous = None
                run = 0
    if len(intermediate) < 1500:
        raise SystemExit(f"{name}: continuous transition strip absent")
    if len(set(intermediate)) < 150:
        raise SystemExit(f"{name}: transition collapsed to discrete lines")
    if maximum_plateau > 3:
        raise SystemExit(
            f"{name}: magnified transition plateau {maximum_plateau}px"
        )

PY
