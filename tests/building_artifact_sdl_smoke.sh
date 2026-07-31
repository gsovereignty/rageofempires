#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-building-artifact.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

frame="$smoke_dir/buildings.bmp"
env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_DISABLE_LEGACY_ASSETS=1 \
    AOE_AUDIT_ANY_MAP_SIZE=1 \
    AOE_FOG=0 \
    AOE_WINDOW_SIZE=800x600 \
    AOE_CAMERA_TILE=1,5 \
    "AOE_SCENARIO_PATH=$script_dir/../resources/water-render-audit.scenario" \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    "AOE_SCREENSHOT_PATH=$frame" \
    "$app_path" >"$smoke_dir/run.log" 2>&1

python3 - "$frame" <<'PY'
import struct
import sys

data = open(sys.argv[1], "rb").read()
if data[:2] != b"BM":
    raise SystemExit("not a BMP")
offset = struct.unpack_from("<I", data, 10)[0]
width = struct.unpack_from("<i", data, 18)[0]
height = abs(struct.unpack_from("<i", data, 22)[0])
bpp = struct.unpack_from("<H", data, 28)[0]
if bpp not in (24, 32):
    raise SystemExit(f"unsupported BMP depth {bpp}")
stride = ((width * bpp + 31) // 32) * 4
forbidden = {
    (77, 78, 68): "opaque gray footprint",
    (52, 76, 135): "opaque blue roof polygon",
}
counts = {color: 0 for color in forbidden}
step = bpp // 8
for y in range(height):
    row = offset + y * stride
    for x in range(width):
        blue, green, red = data[row + x * step:row + x * step + 3]
        color = (red, green, blue)
        if color in counts:
            counts[color] += 1
for color, count in counts.items():
    if count:
        raise SystemExit(f"{forbidden[color]} pixels remain: {count}")
PY
