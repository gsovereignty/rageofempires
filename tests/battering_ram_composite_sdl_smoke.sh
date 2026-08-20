#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-ram-composite.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local tick=$1
    local run=$2
    local output="$smoke_dir/tick-$tick-$run"
    mkdir -p "$output"
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_AUDIT_ANY_MAP_SIZE=1 \
        AOE_FOG=0 \
        AOE_WINDOW_SIZE=1280x720 \
        AOE_CAMERA_TILE=3,5 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/movement-gait-audit.scenario" \
        "AOE_SCREENSHOT_ADVANCE_TICKS=$tick" \
        "AOE_SCREENSHOT_TICK=$tick" \
        "AOE_OVERLAP_CAPTURE_DIR=$output" \
        "AOE_OVERLAP_CAPTURE_TICK=$tick" \
        AOE_OVERLAP_CAPTURE_EXIT=1 \
        "$app_path" >"$smoke_dir/tick-$tick-$run.log" 2>&1
    test -s "$output/manifest.json"
}

for run in 1 2; do
    capture 0 "$run"
    capture 1 "$run"
done

cmp "$smoke_dir/tick-0-1/actual.bmp" "$smoke_dir/tick-0-2/actual.bmp"
cmp "$smoke_dir/tick-1-1/actual.bmp" "$smoke_dir/tick-1-2/actual.bmp"

python3 - "$smoke_dir" <<'PY'
import json
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1])

def ram_case(tick):
    manifest = json.loads((root / f"tick-{tick}-1/manifest.json").read_text())
    return next(case for case in manifest["cases"]
                if case["metadata"]["entity"] == "unit-battering_ram")

def tga_size(case, tick):
    data = (root / f"tick-{tick}-1" / case["sprite"]).read_bytes()
    return struct.unpack_from("<HH", data, 12)

idle = ram_case(0)
moving = ram_case(1)
assert [frame["resource_id"] for frame in idle["metadata"]["sprite_frames"]] == [179]
assert [frame["resource_id"] for frame in moving["metadata"]["sprite_frames"]] == [183, 181]
assert [frame["frame"] for frame in idle["metadata"]["sprite_frames"]] == [2]
assert [frame["frame"] for frame in moving["metadata"]["sprite_frames"]] == [30, 2]
assert all(frame["logical_direction"] == 0
           for case in (idle, moving)
           for frame in case["metadata"]["sprite_frames"])
assert all(frame["stored_direction"] == 2
           for case in (idle, moving)
           for frame in case["metadata"]["sprite_frames"])
assert all(frame["flip_horizontal"] == 1
           for case in (idle, moving)
           for frame in case["metadata"]["sprite_frames"])
assert tga_size(idle, 0) == tga_size(moving, 1) == (120, 80)
assert (idle["x"], idle["y"]) == (moving["x"], moving["y"]) == (105, 101)
PY
