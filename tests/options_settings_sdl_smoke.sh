#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-options.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT
frame="$smoke_dir/options.bmp"

env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_DISABLE_LEGACY_ASSETS=1 \
    AOE_MAIN_MENU=0 \
    AOE_WINDOW_SIZE=1024x768 \
    AOE_OPTIONS_PANEL=1 \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    "AOE_SCREENSHOT_PATH=$frame" \
    "$app_path" >"$smoke_dir/options.log" 2>&1

python3 - "$frame" <<'PY'
import pathlib
import struct
import sys

data = pathlib.Path(sys.argv[1]).read_bytes()
if data[:2] != b"BM":
    raise SystemExit("options capture missing")
width = struct.unpack_from("<i", data, 18)[0]
height = abs(struct.unpack_from("<i", data, 22)[0])
if (width, height) != (1024, 768):
    raise SystemExit(f"options canvas changed: {width}x{height}")
offset = struct.unpack_from("<I", data, 10)[0]
pixels = data[offset:]
# Options panel contains gold text/chrome; blank world capture does not.
gold = sum(1 for i in range(0, len(pixels) - 3, 4)
           if pixels[i + 2] > 150 and pixels[i + 1] > 100)
if gold < 100:
    raise SystemExit("options controls did not render")
PY
