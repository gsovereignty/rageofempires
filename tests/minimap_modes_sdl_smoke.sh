#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-minimap-modes.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local mode=$1
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_DISABLE_LEGACY_ASSETS=1 \
        AOE_WINDOW_SIZE=1280x752 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        AOE_MINIMAP_MODE_PROOF="$mode" \
        AOE_SCREENSHOT_PATH="$smoke_dir/$mode.bmp" \
        "$app_path" >"$smoke_dir/$mode.log" 2>&1
    test -s "$smoke_dir/$mode.bmp"
}

capture normal
capture combat
capture economic
capture economic-statistics

grep -q 'minimap mode proof: NORMAL statistics=0' "$smoke_dir/normal.log"
grep -q 'minimap mode proof: COMBAT statistics=0' "$smoke_dir/combat.log"
grep -q 'minimap mode proof: ECONOMIC statistics=0' "$smoke_dir/economic.log"
grep -q 'minimap mode proof: ECONOMIC statistics=1' \
    "$smoke_dir/economic-statistics.log"

for pair in 'normal combat' 'normal economic' 'economic economic-statistics'; do
    set -- $pair
    if cmp -s "$smoke_dir/$1.bmp" "$smoke_dir/$2.bmp"; then
        echo "minimap mode captures $1 and $2 are identical" >&2
        exit 1
    fi
done

python3 - "$smoke_dir/normal.bmp" "$smoke_dir/combat.bmp" \
    "$smoke_dir/economic.bmp" <<'PY'
import pathlib
import struct
import sys

def crop(path):
    data = pathlib.Path(path).read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    stored_height = struct.unpack_from("<i", data, 22)[0]
    height = abs(stored_height)
    bpp = struct.unpack_from("<H", data, 28)[0]
    stride = ((width * bpp + 31) // 32) * 4
    step = bpp // 8
    pixels = []
    for y in range(height - 171, height - 5):
        source_y = height - 1 - y if stored_height > 0 else y
        for x in range(width - 336, width - 10):
            at = offset + source_y * stride + x * step
            pixels.append(data[at:at + 3])
    return b"".join(pixels)

crops = [crop(path) for path in sys.argv[1:]]
if len(set(crops)) != 3:
    raise SystemExit("mode-dependent minimap crop did not change")
PY
