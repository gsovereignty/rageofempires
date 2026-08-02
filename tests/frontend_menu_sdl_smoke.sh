#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-frontend-menu.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    name=$1
    size=$2
    shift 2
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_DISABLE_LEGACY_ASSETS=1 \
        "AOE_WINDOW_SIZE=$size" \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$name.bmp" \
        "$@" \
        "$app_path" >"$smoke_dir/$name.log" 2>&1
}

capture main 800x600 env AOE_MAIN_MENU=1
capture proof-click 800x600 env \
    AOE_MAIN_MENU=1 AOE_MENU_ACTIVATION_PROOF=click
capture proof-enter 800x600 env \
    AOE_MAIN_MENU=1 AOE_MENU_ACTIVATION_PROOF=enter
for focus in 0 1 2 3 4 5 6; do
    capture "single-$focus" 800x600 env \
        AOE_MENU_REFERENCE=1 "AOE_MENU_FOCUS=$focus"
done
capture wide 1280x720 env AOE_MENU_REFERENCE=1 AOE_MENU_FOCUS=0
capture original 800x600 env \
    AOE_DISABLE_LEGACY_ASSETS=0 \
    AOE_MENU_REFERENCE=1 \
    AOE_MENU_FOCUS=0

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
    height = abs(struct.unpack_from("<i", data, 22)[0])
    bpp = struct.unpack_from("<H", data, 28)[0]
    stride = ((width * bpp + 31) // 32) * 4
    step = bpp // 8

    def pixel(x, y):
        row = offset + (height - 1 - y) * stride
        blue, green, red = data[
            row + x * step:row + x * step + 3
        ]
        return red, green, blue

    return width, height, pixel


root = pathlib.Path(sys.argv[1])
width, height, main = read(root / "main.bmp")
if (width, height) != (800, 600):
    raise SystemExit("main menu capture is not exact 800x600")
if main(500, 100) == (0, 0, 0):
    raise SystemExit("main menu did not render full logical canvas")

for proof in ("proof-click", "proof-enter"):
    _, _, activated = read(root / f"{proof}.bmp")
    # Single-player screen places first control on right; main menu does not.
    if activated(500, 100) == main(500, 100):
        raise SystemExit(f"{proof}: Single Player transition not visible")

buttons = [
    (476, 89, 260, 39),
    (476, 153, 260, 39),
    (476, 204, 260, 39),
    (476, 254, 260, 39),
    (476, 319, 260, 39),
    (476, 369, 260, 39),
    (476, 434, 260, 39),
]
focus_color = (202, 207, 1)
for index, (x, y, w, h) in enumerate(buttons):
    width, height, pixel = read(root / f"single-{index}.bmp")
    if (width, height) != (800, 600):
        raise SystemExit(f"focus {index}: wrong canvas")
    focused = sum(
        pixel(px, py) == focus_color
        for py in range(y, y + h)
        for px in range(x, x + w)
    )
    if focused < 8:
        raise SystemExit(f"focus {index}: focused text color absent")

width, height, wide = read(root / "wide.bmp")
if (width, height) != (1280, 720):
    raise SystemExit("wide capture dimensions wrong")
if wide(0, 360) == (10, 9, 7) or wide(1279, 360) == (10, 9, 7):
    raise SystemExit("wide menu exposes black side gutters")
if wide(640, 2) == (10, 9, 7):
    raise SystemExit("wide menu logical canvas does not cover window")

width, height, original = read(root / "original.bmp")
if (width, height) != (800, 600):
    raise SystemExit("original-asset capture is not exact 800x600")
if original(200, 300) == main(200, 300):
    raise SystemExit("original menu asset did not replace fallback art")
log = (root / "original.log").read_text(errors="replace")
if "using original menu SLP" not in log:
    raise SystemExit("original menu SLP load not confirmed")
PY
