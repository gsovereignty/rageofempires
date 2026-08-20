#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-villager-death.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local tick=$1
    local run=$2
    local output="$smoke_dir/tick-$tick-$run"
    mkdir -p "$output"
    env -u AOE_DISABLE_LEGACY_ASSETS \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_AUDIT_ANY_MAP_SIZE=1 \
        AOE_FOG=0 \
        AOE_WINDOW_SIZE=1280x720 \
        AOE_CAMERA_TILE=3,2 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/combat-pose-audit.scenario" \
        "AOE_SCREENSHOT_ADVANCE_TICKS=$tick" \
        "AOE_SCREENSHOT_TICK=$tick" \
        "AOE_OVERLAP_CAPTURE_DIR=$output" \
        "AOE_OVERLAP_CAPTURE_TICK=$tick" \
        AOE_OVERLAP_CAPTURE_EXIT=1 \
        "$app_path" >"$smoke_dir/tick-$tick-$run.log" 2>&1
    test -s "$output/manifest.json"
}

for run in 1 2; do
    capture 12 "$run"
    capture 18 "$run"
done

cmp "$smoke_dir/tick-12-1/actual.bmp" "$smoke_dir/tick-12-2/actual.bmp"
cmp "$smoke_dir/tick-18-1/actual.bmp" "$smoke_dir/tick-18-2/actual.bmp"

python3 - "$smoke_dir" <<'PY'
import json
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1])

def cases(tick):
    return json.loads(
        (root / f"tick-{tick}-1/manifest.json").read_text()
    )["cases"]

def villager_death(tick):
    matches = [case for case in cases(tick)
               if case["metadata"]["entity"] == "unit-death-villager"]
    assert len(matches) == 1
    return matches[0]

falling = villager_death(12)
corpse = villager_death(18)
assert falling["metadata"]["entity"] == "unit-death-villager"
assert corpse["metadata"]["entity"] == "unit-death-villager"
assert falling["metadata"]["entity_id"] == 3
assert corpse["metadata"]["entity_id"] == 3
assert len(falling["metadata"]["sprite_frames"]) == 1
assert len(corpse["metadata"]["sprite_frames"]) == 1
falling_frame = falling["metadata"]["sprite_frames"][0]
corpse_frame = corpse["metadata"]["sprite_frames"][0]
assert {
    key: falling_frame[key]
    for key in ("resource_id", "frame", "palette_player",
                "flip_horizontal", "logical_direction",
                "stored_direction", "action_frame")
} == {
    "resource_id": 1476,
    "frame": 33,
    "palette_player": 2,
    "flip_horizontal": 1,
    "logical_direction": 0,
    "stored_direction": 2,
    "action_frame": 3,
}
assert {
    key: corpse_frame[key]
    for key in ("resource_id", "frame", "palette_player",
                "flip_horizontal", "logical_direction",
                "stored_direction", "action_frame")
} == {
    "resource_id": 1476,
    "frame": 44,
    "palette_player": 2,
    "flip_horizontal": 1,
    "logical_direction": 0,
    "stored_direction": 2,
    "action_frame": 14,
}
assert (falling["x"], falling["y"]) == (282, 106)
assert (corpse["x"], corpse["y"]) == (246, 120)
assert struct.unpack_from(
    "<HH", (root / "tick-12-1" / falling["sprite"]).read_bytes(), 12
) == (42, 41)
assert struct.unpack_from(
    "<HH", (root / "tick-18-1" / corpse["sprite"]).read_bytes(), 12
) == (55, 35)
assert not any(case["metadata"]["entity"].startswith("building-rubble")
               for tick in (12, 18) for case in cases(tick))
PY
