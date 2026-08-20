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

python3 - "$smoke_dir/before.bmp" "$smoke_dir/after.bmp" <<'PY'
import pathlib
import struct
import sys

def pixels(path):
    data = pathlib.Path(path).read_bytes()
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

    return pixel

before_pixel = pixels(sys.argv[1])
after_pixel = pixels(sys.argv[2])
panel = [
    after_pixel(x, y)
    for y in range(581, 747)
    for x in range(944, 1270)
]
if panel.count((0, 0, 255)) < 20:
    raise SystemExit("blue minimap marker is missing or too small")

# SDL software rendering composites the alpha-235 viewport color over the
# minimap. Detect its resulting warm outline inside the map image, then prove
# that the outline moves with the camera rather than accepting unrelated HUD
# text using the same palette family.
def viewport_pixels(pixel):
    return [
        (x, y)
        for y in range(588, 722)
        for x in range(951, 1263)
        if (lambda color:
            color[0] >= 220 and color[1] >= 205 and color[2] >= 165 and
            color[0] > color[1] > color[2]
        )(pixel(x, y))
    ]

before_viewport = viewport_pixels(before_pixel)
after_viewport = viewport_pixels(after_pixel)
if len(before_viewport) < 40 or len(after_viewport) < 40:
    raise SystemExit("minimap viewport rectangle is missing")
before_center_x = sum(x for x, _ in before_viewport) / len(before_viewport)
after_center_x = sum(x for x, _ in after_viewport) / len(after_viewport)
if after_center_x - before_center_x < 100:
    raise SystemExit("minimap viewport rectangle did not follow camera")
if not any(g >= 130 and g > r and g > b for r, g, b in panel):
    raise SystemExit("minimap explored terrain/route lacks readable green")
PY
