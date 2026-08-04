#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-building-contact.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local run=$1
    local output="$smoke_dir/run-$run"
    mkdir -p "$output"
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_AUDIT_ANY_MAP_SIZE=1 \
        AOE_FOG=0 \
        AOE_WINDOW_SIZE=1280x720 \
        AOE_CAMERA_TILE=5,2 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/ai-building-attack-audit.scenario" \
        AOE_SCREENSHOT_ADVANCE_TICKS=40 \
        AOE_SCREENSHOT_TICK=40 \
        "AOE_OVERLAP_CAPTURE_DIR=$output" \
        AOE_OVERLAP_CAPTURE_TICK=40 \
        AOE_OVERLAP_CAPTURE_EXIT=1 \
        "$app_path" >"$smoke_dir/run-$run.log" 2>&1
    test -s "$output/manifest.json"
}

capture 1
capture 2
cmp "$smoke_dir/run-1/actual.bmp" "$smoke_dir/run-2/actual.bmp"

python3 - "$smoke_dir/run-1" <<'PY'
import json
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1])
manifest = json.loads((root / "manifest.json").read_text())
ram = next(case for case in manifest["cases"]
           if case["metadata"]["entity"] == "unit-battering_ram")
assert [frame["resource_id"] for frame in
        ram["metadata"]["sprite_frames"]] == [173, 171]

tga = (root / ram["sprite"]).read_bytes()
width, height = struct.unpack_from("<HH", tga, 12)
assert tga[2] == 2 and tga[16] == 32 and tga[17] & 0x20
sprite = tga[18:]

bmp = (root / "actual.bmp").read_bytes()
bmp_width, bmp_height = struct.unpack_from("<ii", bmp, 18)
bmp_offset = struct.unpack_from("<I", bmp, 10)[0]
bmp_stride = ((bmp_width * 3 + 3) // 4) * 4

opaque = 0
visible = 0
for y in range(height):
    for x in range(width):
        sprite_offset = (y * width + x) * 4
        blue, green, red, alpha = sprite[sprite_offset:sprite_offset + 4]
        if alpha == 0:
            continue
        opaque += 1
        actual_x = ram["x"] + x
        actual_y = ram["y"] + y
        actual_offset = (bmp_offset +
                         (bmp_height - 1 - actual_y) * bmp_stride +
                         actual_x * 3)
        if bmp[actual_offset:actual_offset + 3] == bytes((blue, green, red)):
            visible += 1

assert opaque > 7000
assert visible == opaque, (visible, opaque)
PY
