#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-building-construction.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

for percent in complete 25 75; do
    audit=()
    if [ "$percent" != complete ]; then
        audit=("AOE_CONSTRUCTION_AUDIT_PERCENT=$percent")
    fi
    env -u AOE_DISABLE_LEGACY_ASSETS \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_AUDIT_ANY_MAP_SIZE=1 \
        AOE_FOG=0 \
        AOE_WINDOW_SIZE=800x600 \
        AOE_CAMERA_TILE=8,5 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/building-age-dark.scenario" \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$percent.bmp" \
        "${audit[@]}" \
        "$app_path" >"$smoke_dir/$percent.log" 2>&1
done

python3 - "$smoke_dir" <<'PY'
import pathlib
import struct
import sys


def pixels(path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise SystemExit(f"{path.name}: not BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = abs(struct.unpack_from("<i", data, 22)[0])
    bpp = struct.unpack_from("<H", data, 28)[0]
    stride = ((width * bpp + 31) // 32) * 4
    step = bpp // 8
    result = []
    for row_index in range(height):
        row = offset + row_index * stride
        for column in range(width):
            result.append(tuple(data[row + column * step:row + column * step + 3]))
    return result


root = pathlib.Path(sys.argv[1])
complete = pixels(root / "complete.bmp")
early = pixels(root / "25.bmp")
late = pixels(root / "75.bmp")

early_changed = sum(a != b for a, b in zip(early, complete))
late_changed = sum(a != b for a, b in zip(late, complete))
between = sum(a != b for a, b in zip(early, late))
if between < 100:
    raise SystemExit(
        f"construction progress produced no material body change: {between}"
    )
if not late_changed < early_changed:
    raise SystemExit(
        f"late construction did not converge on completed bodies: "
        f"early={early_changed}, late={late_changed}"
    )
PY
