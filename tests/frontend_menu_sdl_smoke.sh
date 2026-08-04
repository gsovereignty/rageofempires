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
capture proof-ai-tiny 800x600 env \
    AOE_MAIN_MENU=1 AOE_MENU_ACTIVATION_PROOF=ai-tiny
capture proof-learn 800x600 env \
    AOE_MAIN_MENU=1 AOE_MENU_ACTIVATION_PROOF=learn
capture proof-regicide 800x600 env \
    AOE_MAIN_MENU=1 AOE_MENU_ACTIVATION_PROOF=regicide
capture proof-death-match 800x600 env \
    AOE_MAIN_MENU=1 AOE_MENU_ACTIVATION_PROOF=death-match
capture proof-zone 800x600 env \
    AOE_MAIN_MENU=1 AOE_MENU_ACTIVATION_PROOF=zone
for focus in 0 1 2 3 4 5 6; do
    capture "single-$focus" 800x600 env \
        AOE_MENU_REFERENCE=1 "AOE_MENU_FOCUS=$focus"
done
capture wide 1280x720 env AOE_MENU_REFERENCE=1 AOE_MENU_FOCUS=0
capture original 800x600 env \
    AOE_DISABLE_LEGACY_ASSETS=0 \
    AOE_MENU_REFERENCE=1 \
    AOE_MENU_FOCUS=0
capture native-normal 1366x768 env \
    AOE_DISABLE_LEGACY_ASSETS=0 AOE_MAIN_MENU=1 AOE_MENU_FOCUS=4
capture native-focused 1366x768 env \
    AOE_DISABLE_LEGACY_ASSETS=0 AOE_MAIN_MENU=1 AOE_MENU_FOCUS=1
capture native-pressed 1366x768 env \
    AOE_DISABLE_LEGACY_ASSETS=0 AOE_MAIN_MENU=1 AOE_MENU_FOCUS=1 \
    AOE_NATIVE_MENU_PRESSED=0
capture native-four-three 1024x768 env \
    AOE_DISABLE_LEGACY_ASSETS=0 AOE_MAIN_MENU=1 AOE_MENU_FOCUS=1
capture native-click 1366x768 env \
    AOE_DISABLE_LEGACY_ASSETS=0 AOE_MAIN_MENU=1 \
    AOE_MENU_ACTIVATION_PROOF=native-single

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
    if (root / f"{proof}.bmp").read_bytes() == (
        root / "main.bmp"
    ).read_bytes():
        raise SystemExit(f"{proof}: Single Player transition not visible")

if (root / "proof-ai-tiny.bmp").read_bytes() == (
    root / "main.bmp"
).read_bytes():
    raise SystemExit("ai-tiny: direct Arabia match did not replace menu")
if "launched 1v1 Arabia vs AI: TINY" not in (
    root / "proof-ai-tiny.log"
).read_text(errors="replace"):
    raise SystemExit("ai-tiny: exact map-size launch not confirmed")
for proof in ("proof-learn", "proof-regicide", "proof-death-match"):
    if (root / f"{proof}.bmp").read_bytes() == (
        root / "main.bmp"
    ).read_bytes():
        raise SystemExit(f"{proof}: playable mode did not replace menu")
if (root / "proof-zone.bmp").read_bytes() == (root / "main.bmp").read_bytes():
    raise SystemExit("zone: retired-service presentation did not open")

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

native_normal_path = root / "native-normal.bmp"
native_focused_path = root / "native-focused.bmp"
native_pressed_path = root / "native-pressed.bmp"
width, height, native_normal = read(native_normal_path)
if (width, height) != (1366, 768):
    raise SystemExit("native menu did not retain exact design canvas")
for name in ("native-normal", "native-focused", "native-pressed",
             "native-four-three", "native-click"):
    if "using original menu SLP" not in (
        root / f"{name}.log"
    ).read_text(errors="replace"):
        raise SystemExit(f"{name}: archive-backed proof did not opt in")
if native_normal_path.read_bytes() == native_focused_path.read_bytes():
    raise SystemExit("native focused frame did not change pixels")
if native_focused_path.read_bytes() == native_pressed_path.read_bytes():
    raise SystemExit("native pressed frame did not change pixels")

def color_count(pixel, bounds, color):
    x, y, width, height = bounds
    return sum(
        pixel(px, py) == color
        for py in range(y, y + height)
        for px in range(x, x + width)
    )

single_label = (542, 20, 178, 38)
learn_label = (150, 13, 188, 40)
if color_count(native_normal, single_label, (217, 208, 176)) < 2:
    raise SystemExit("native normal label color/geometry absent")
_, _, native_focused = read(native_focused_path)
if color_count(native_focused, single_label, (202, 207, 1)) < 2:
    raise SystemExit("native focused label color/geometry absent")
if color_count(native_normal, learn_label, (217, 208, 176)) < 2:
    raise SystemExit("native enabled Learn to Play label absent")
if color_count(native_normal, single_label, (0, 0, 0)) < 2:
    raise SystemExit("native label shadow absent")

width, height, native_four_three = read(root / "native-four-three.bmp")
if (width, height) != (1024, 768):
    raise SystemExit("native alternate-resolution capture dimensions wrong")
if native_four_three(512, 20) != (10, 9, 7):
    raise SystemExit("native alternate-resolution top letterbox absent")
if native_four_three(512, 384) == (10, 9, 7):
    raise SystemExit("native alternate-resolution canvas absent")

if (root / "native-click.bmp").read_bytes() == native_focused_path.read_bytes():
    raise SystemExit("native alpha-mask click did not open Single Player")
PY
