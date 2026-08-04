#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-hud-layout.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local size=$1
    local assets=$2
    local scale=$3
    local panel=${4:-unit}
    local proof=${5:-}
    local name="${size}-${assets}-${scale}x-${panel}${proof:+-$proof}"
    local disabled=1
    if [[ "$assets" == icons ]]; then
        disabled=0
    fi
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        "AOE_DISABLE_LEGACY_ASSETS=$disabled" \
        "AOE_WINDOW_SIZE=$size" \
        "AOE_HUD_OUTPUT_SCALE=$scale" \
        AOE_HUD_STRESS_VALUES=1 \
        "AOE_COMMAND_VISUAL_PROOF=$proof" \
        "AOE_COMMAND_PANEL=$panel" \
        AOE_SELECTION_PROOF=1 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$name.bmp" \
        "$app_path" >"$smoke_dir/$name.log" 2>&1
}

for size in 640x480 800x600 1024x768 1280x720 1920x1080; do
    capture "$size" fallback 1
done
capture 640x480 fallback 2
capture 1280x720 fallback 1 building
capture 1280x720 icons 1
capture 1280x720 icons 1 unit normal
capture 1280x720 icons 1 unit pressed
capture 1280x720 icons 1 unit disabled
! cmp -s \
    "$smoke_dir/1280x720-fallback-1x-unit.bmp" \
    "$smoke_dir/1280x720-fallback-1x-building.bmp"
! cmp -s \
    "$smoke_dir/1280x720-icons-1x-unit-normal.bmp" \
    "$smoke_dir/1280x720-icons-1x-unit-pressed.bmp"
! cmp -s \
    "$smoke_dir/1280x720-icons-1x-unit-normal.bmp" \
    "$smoke_dir/1280x720-icons-1x-unit-disabled.bmp"

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

    return width, height, pixel


def layout(screen_width, icons):
    margin = 2
    gap = 2
    row_width = min(420, max(0, screen_width - margin * 2))
    available = max(0, row_width - gap * 4)
    weights = (3, 3, 3, 3, 4)
    fields = []
    x = margin
    assigned = 0
    for index, weight in enumerate(weights):
        width = (
            available - assigned
            if index + 1 == len(weights)
            else available * weight // sum(weights)
        )
        assigned += width
        text_x = x + (16 if icons and index < 4 else 2)
        fields.append((x, width, text_x, x + width - text_x))
        x += width + gap
    return row_width, fields


root = pathlib.Path(sys.argv[1])
foreground = (239, 226, 185)
cases = [
    (640, 480, "fallback", 1),
    (800, 600, "fallback", 1),
    (1024, 768, "fallback", 1),
    (1280, 720, "fallback", 1),
    (1280, 720, "icons", 1),
    (1920, 1080, "fallback", 1),
    (640, 480, "fallback", 2),
]
for logical_width, logical_height, assets, scale in cases:
    name = (
        f"{logical_width}x{logical_height}-{assets}-{scale}x-unit"
    )
    width, height, pixel = read(root / f"{name}.bmp")
    expected = (logical_width * scale, logical_height * scale)
    if (width, height) != expected:
        raise SystemExit(
            f"{name}: output {(width, height)} != {expected}"
        )
    canonical_width = logical_width * scale
    row_width, fields = layout(canonical_width, assets == "icons")
    for index, (x, field_width, text_x, text_width) in enumerate(fields):
        text_pixels = sum(
            pixel(px, py) == foreground
            for py in range(6, 14)
            for px in range(text_x, text_x + text_width)
        )
        if text_pixels == 0:
            raise SystemExit(f"{name}: field {index} has no text pixels")
        if index:
            previous_end = fields[index - 1][0] + fields[index - 1][1]
            gap_pixels = sum(
                pixel(px, py) == foreground
                for py in range(2, 18)
                for px in range(previous_end, x)
            )
            if gap_pixels:
                raise SystemExit(
                    f"{name}: field {index - 1} crossed guard band"
                )
    row_end = 2 + row_width
    leaked_right = sum(
        pixel(px, py) == foreground
        for py in range(2, 18)
        for px in range(row_end, width)
    )
    if leaked_right:
        raise SystemExit(f"{name}: resource row crossed right bound")
    log = (root / f"{name}.log").read_text(errors="replace")
    expected_log = (
        f"output-scale={scale} row=2,2,{row_width},16"
    )
    if expected_log not in log:
        raise SystemExit(f"{name}: presentation geometry not logged")
PY
