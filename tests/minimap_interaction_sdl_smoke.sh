#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-minimap-interaction.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local name=$1
    shift
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_DISABLE_LEGACY_ASSETS=1 \
        AOE_WINDOW_SIZE=1280x752 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$name.bmp" \
        "$@" \
        "$app_path" >"$smoke_dir/$name.log" 2>&1
}

capture before env
capture after env AOE_MINIMAP_CLICK_PROOF=1
if cmp -s "$smoke_dir/before.bmp" "$smoke_dir/after.bmp"; then
    echo "minimap click did not change visible camera" >&2
    exit 1
fi
grep -q "minimap click centered camera on" "$smoke_dir/after.log"

python3 - "$smoke_dir/after.bmp" <<'PY'
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
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

panel = [
    pixel(x, y)
    for y in range(581, 747)
    for x in range(944, 1270)
]
if panel.count((0, 0, 255)) < 20:
    raise SystemExit("blue minimap marker is missing or too small")
if not any(r >= 235 and g >= 220 and b >= 180 for r, g, b in panel):
    raise SystemExit("minimap viewport rectangle is missing")
if not any(g >= 150 and r < 140 for r, g, b in panel):
    raise SystemExit("minimap explored terrain/route lacks readable green")
PY
