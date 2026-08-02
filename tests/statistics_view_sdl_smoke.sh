#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-statistics-view.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

for tab in economy military society technology timeline; do
    for repeat in 1 2; do
        env \
            SDL_VIDEODRIVER=dummy \
            SDL_AUDIODRIVER=dummy \
            SDL_RENDER_DRIVER=software \
            AOE_MAIN_MENU=0 \
            AOE_DISABLE_LEGACY_ASSETS=1 \
            AOE_WINDOW_SIZE=1280x752 \
            AOE_STATISTICS_PANEL=1 \
            "AOE_STATISTICS_CLICK_PROOF=$tab" \
            AOE_SCREENSHOT_TICK=1 \
            AOE_EXIT_AFTER_SCREENSHOT=1 \
            "AOE_SCREENSHOT_PATH=$smoke_dir/$tab-$repeat.bmp" \
            "$app_path" >"$smoke_dir/$tab-$repeat.log" 2>&1
    done
done

for repeat in 1 2 3 4; do
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_DISABLE_LEGACY_ASSETS=0 \
        AOE_WINDOW_SIZE=1280x752 \
        AOE_STATISTICS_PANEL=1 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/original-$repeat.bmp" \
        "$app_path" >"$smoke_dir/original-$repeat.log" 2>&1
done

for assets in fallback original; do
    disabled=0
    if [[ "$assets" == fallback ]]; then disabled=1; fi
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        "AOE_DISABLE_LEGACY_ASSETS=$disabled" \
        AOE_WINDOW_SIZE=1280x752 \
        AOE_STATISTICS_POSTGAME_PROOF=1 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/postgame-$assets.bmp" \
        "$app_path" >"$smoke_dir/postgame-$assets.log" 2>&1
done

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

    return data, width, height, pixel


root = pathlib.Path(sys.argv[1])
tabs = ("economy", "military", "society", "technology", "timeline")
selected_fill = (128, 83, 29)
inactive_fill = (66, 54, 37)
for selected, tab in enumerate(tabs):
    first_data, width, height, pixel = read(root / f"{tab}-1.bmp")
    second_data, width2, height2, pixel2 = read(root / f"{tab}-2.bmp")
    if (width, height) != (1280, 752) or (width2, height2) != (1280, 752):
        raise SystemExit(f"{tab}: incorrect capture dimensions")
    if first_data != second_data:
        raise SystemExit(f"{tab}: repeated render is not deterministic")
    # Full panel corners/header must exist. This catches partial black frames.
    for point in ((120, 36), (1159, 36), (120, 675), (1159, 675), (500, 70)):
        if pixel(*point) == (0, 0, 0):
            raise SystemExit(f"{tab}: incomplete black frame at {point}")
    for index in range(5):
        sample = pixel(145 + index * 190, 115)
        expected = selected_fill if index == selected else inactive_fill
        if sample != expected:
            raise SystemExit(
                f"{tab}: click selected tab {selected}, fill {index}={sample}, expected {expected}"
            )
    # Header separators and player banners remain visible on every tab.
    if pixel(450, 60) == pixel(500, 60):
        raise SystemExit(f"{tab}: header composition lacks separation")
    if pixel(750, 153) == pixel(915, 153):
        raise SystemExit(f"{tab}: player banners are indistinguishable")

original, width, height, original_pixel = read(root / "original-1.bmp")
if "using original statistics interface SLPs" not in (
    root / "original-1.log"
).read_text(errors="replace"):
    raise SystemExit("original statistics SLP load not confirmed")
for repeat in range(2, 5):
    repeated, repeated_width, repeated_height, _ = read(
        root / f"original-{repeat}.bmp"
    )
    if (repeated_width, repeated_height) != (width, height):
        raise SystemExit("original statistics capture dimensions changed")
    if repeated != original:
        raise SystemExit(
            f"original statistics render {repeat} is partial or nondeterministic"
        )
if original == read(root / "economy-1.bmp")[0]:
    raise SystemExit("original statistics art did not replace fallback")
for point in ((120, 36), (1159, 36), (120, 675), (1159, 675), (500, 70)):
    if original_pixel(*point) == (0, 0, 0):
        raise SystemExit(f"original statistics frame incomplete at {point}")

for assets in ("fallback", "original"):
    _, width, height, postgame = read(root / f"postgame-{assets}.bmp")
    if (width, height) != (1280, 752):
        raise SystemExit(f"postgame {assets}: wrong dimensions")
    # Three action button centers must differ from surrounding panel.
    for center_x in (420, 640, 860):
        if postgame(center_x, 633) == postgame(center_x, 600):
            raise SystemExit(f"postgame {assets}: action button absent")
if "using original statistics interface SLPs" not in (
    root / "postgame-original.log"
).read_text(errors="replace"):
    raise SystemExit("postgame original button art load not confirmed")
PY
