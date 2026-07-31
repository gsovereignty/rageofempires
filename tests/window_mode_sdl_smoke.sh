#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-window-mode.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

log_path="$smoke_dir/window.log"
bmp_path="$smoke_dir/window.bmp"
env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_MAIN_MENU=0 \
    AOE_DISABLE_LEGACY_ASSETS=1 \
    AOE_WINDOW_SIZE=800x600 \
    AOE_WINDOW_RESIZE_PROOF=960x540 \
    AOE_FULLSCREEN_ROUNDTRIP_PROOF=1 \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    "AOE_SCREENSHOT_PATH=$bmp_path" \
    "$app_path" >"$log_path" 2>&1

python3 - "$bmp_path" "$log_path" <<'PY'
import pathlib
import re
import struct
import sys

bmp = pathlib.Path(sys.argv[1]).read_bytes()
if bmp[:2] != b"BM":
    raise SystemExit("resize proof did not produce a BMP")
width = struct.unpack_from("<i", bmp, 18)[0]
height = abs(struct.unpack_from("<i", bmp, 22)[0])
if (width, height) == (800, 600) or width < 640 or height < 360:
    raise SystemExit(f"live resize capture did not become usable: {width}x{height}")

log = pathlib.Path(sys.argv[2]).read_text(errors="replace")
match = re.search(
    r"Resize proof window=960x540 drawable=(\d+)x(\d+)", log
)
if not match or tuple(map(int, match.groups())) != (width, height):
    raise SystemExit("live resize did not update canonical drawable extent")
if "Fullscreen roundtrip " not in log:
    raise SystemExit("fullscreen round-trip/rollback path was not exercised")
if not (
    "entered=1 left=1 live=0" in log
    or "entered=0 left=0 live=0" in log
):
    raise SystemExit("fullscreen proof neither round-tripped nor rolled back")
PY
