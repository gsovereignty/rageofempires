#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-elevation-render.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    name=$1
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_FOG=0 \
        AOE_WINDOW_SIZE=800x600 \
        AOE_CAMERA_TILE=8,8 \
        AOE_CAMERA_ZOOM=1.0 \
        AOE_TERRAIN_ARCHIVE_ONLY=1 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/elevation-transition-matrix.scenario" \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$name.bmp" \
        "$app_path" >"$smoke_dir/$name.log" 2>&1
}

capture first
capture second
cmp "$smoke_dir/first.bmp" "$smoke_dir/second.bmp"
grep -q 'using exact classic FilterMaps.dat slope scanlines' "$smoke_dir/first.log"

python3 - "$smoke_dir/first.bmp" <<'PY'
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
if data[:2] != b"BM":
    raise SystemExit("elevation capture is not BMP")
offset = struct.unpack_from("<I", data, 10)[0]
width = struct.unpack_from("<i", data, 18)[0]
height = abs(struct.unpack_from("<i", data, 22)[0])
bpp = struct.unpack_from("<H", data, 28)[0]
if (width, height) != (800, 600) or bpp not in (24, 32):
    raise SystemExit("elevation capture dimensions changed")
pixels = data[offset:]
step = bpp // 8
if len(set(pixels[index:index + step] for index in range(0, len(pixels), step))) < 256:
    raise SystemExit("elevation capture lost filtered terrain variation")
PY
