#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-fog-terrain.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    name=$1
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_FOG=1 \
        AOE_WINDOW_SIZE=800x600 \
        AOE_CAMERA_TILE=8,8 \
        AOE_CAMERA_ZOOM=1.0 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/elevation-transition-matrix.scenario" \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$name.bmp" \
        "$app_path" >"$smoke_dir/$name.log" 2>&1
}

capture first
capture second
cmp "$smoke_dir/first.bmp" "$smoke_dir/second.bmp"
grep -q 'using packaged HD terrain textures from' "$smoke_dir/first.log"

python3 - "$smoke_dir/first.bmp" <<'PY'
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
if data[:2] != b"BM":
    raise SystemExit("fog terrain capture is not BMP")
offset = struct.unpack_from("<I", data, 10)[0]
width = struct.unpack_from("<i", data, 18)[0]
stored_height = struct.unpack_from("<i", data, 22)[0]
height = abs(stored_height)
bpp = struct.unpack_from("<H", data, 28)[0]
if (width, height) != (800, 600) or bpp not in (24, 32):
    raise SystemExit("fog terrain capture dimensions changed")
stride = ((width * bpp + 31) // 32) * 4
step = bpp // 8

def pixel(x, y):
    source_y = height - 1 - y if stored_height > 0 else y
    at = offset + source_y * stride + x * step
    blue, green, red = data[at:at + 3]
    return red, green, blue

world = {
    (x, y): pixel(x, y)
    for y in range(25, 425)
    for x in range(width)
}
black = {
    point for point, color in world.items()
    if max(color) < 35
}
white = sum(min(color) > 235 for color in world.values())
grass = {
    point for point, (red, green, blue) in world.items()
    if green > red * 1.08 and green > blue * 1.08 and green > 70
}

# Real scenario vision leaves black unexplored shroud around textured grass.
if len(black) < 80_000:
    raise SystemExit(f"fog no longer conceals hidden terrain: {len(black)}")
if len(grass) < 35_000:
    raise SystemExit(f"visible grass lost packaged texture/chroma: {len(grass)}")
if white > 1_000:
    raise SystemExit(f"fog replaced terrain with near-white scanlines: {white}")

# Both sides of a shroud transition must occur in-world.  This rejects an
# unbounded black overlay while proving boundary tiles meet concealed area.
contacts = 0
for x, y in grass:
    if any((x + dx, y + dy) in black for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1))):
        contacts += 1
if not 20 <= contacts <= 5_000:
    raise SystemExit(f"fog boundary is missing or unbounded: {contacts}")
PY
