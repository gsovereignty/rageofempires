#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-building-age.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

for age in dark feudal castle imperial; do
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_DISABLE_LEGACY_ASSETS=1 \
        AOE_AUDIT_ANY_MAP_SIZE=1 \
        AOE_FOG=0 \
        AOE_WINDOW_SIZE=800x600 \
        AOE_CAMERA_TILE=8,5 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/building-age-$age.scenario" \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$age.bmp" \
        "$app_path" >"$smoke_dir/$age.log" 2>&1
done

python3 - "$smoke_dir" <<'PY'
import pathlib
import struct
import sys


def pixels(path):
    data = path.read_bytes()
    if data[:2] != b"BM":
        raise SystemExit(f"{path.name}: not a BMP")
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = abs(struct.unpack_from("<i", data, 22)[0])
    bpp = struct.unpack_from("<H", data, 28)[0]
    if (width, height) != (800, 600) or bpp not in (24, 32):
        raise SystemExit(
            f"{path.name}: unexpected geometry {width}x{height}x{bpp}"
        )
    stride = ((width * bpp + 31) // 32) * 4
    step = bpp // 8
    result = {}
    for y in range(25, 425):
        row = offset + y * stride
        for x in range(width):
            blue, green, red = data[
                row + x * step:row + x * step + 3
            ]
            result[x, y] = (red, green, blue)
    return result


def components(points):
    points = set(points)
    result = []
    while points:
        seed = points.pop()
        pending = [seed]
        component = [seed]
        while pending:
            x, y = pending.pop()
            for neighbor in (
                (x - 1, y), (x + 1, y),
                (x, y - 1), (x, y + 1),
            ):
                if neighbor in points:
                    points.remove(neighbor)
                    pending.append(neighbor)
                    component.append(neighbor)
        result.append(component)
    return result


root = pathlib.Path(sys.argv[1])
captures = {
    age: pixels(root / f"{age}.bmp")
    for age in ("dark", "feudal", "castle", "imperial")
}

# Hermetic renderer uses bounded reconstruction-native silhouettes. Require
# coherent body-colored components and reject an oversized nested silhouette.
body_colors = {
    (91, 63, 39), (54, 38, 25), (66, 45, 29),
    (63, 43, 28), (141, 42, 36), (52, 76, 135),
}
for age, capture in captures.items():
    body = [point for point, color in capture.items() if color in body_colors]
    if len(body) < 1200:
        raise SystemExit(f"{age}: building silhouette occupancy too small")
    large = [part for part in components(body) if len(part) >= 40]
    if len(large) < 4:
        raise SystemExit(f"{age}: expected coherent building bodies missing")
    for part in large:
        xs = [point[0] for point in part]
        ys = [point[1] for point in part]
        if max(xs) - min(xs) > 180 or max(ys) - min(ys) > 170:
            raise SystemExit(f"{age}: oversized/nested building silhouette")

# Exact root-ID transition assertions live in render_asset_coverage_tests.
# With legacy assets disabled, every age uses the same age-neutral procedural
# fallback. Require exact gameplay-pixel stability so this hermetic test never
# mistakes synthetic decoration for production age-specific building art.
dark = captures["dark"]
for age in ("feudal", "castle", "imperial"):
    changed = [
        point for point in dark
        if dark[point] != captures[age][point]
    ]
    if changed:
        raise SystemExit(
            f"{age}: age-neutral fallback changed at {len(changed)} pixels"
        )
PY
