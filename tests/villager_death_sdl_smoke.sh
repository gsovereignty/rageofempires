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
    capture 10 "$run"
    capture 16 "$run"
done

cmp "$smoke_dir/tick-10-1/actual.bmp" "$smoke_dir/tick-10-2/actual.bmp"
cmp "$smoke_dir/tick-16-1/actual.bmp" "$smoke_dir/tick-16-2/actual.bmp"

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
               if case["metadata"]["entity_id"] == 2]
    assert len(matches) == 1
    return matches[0]

falling = villager_death(10)
corpse = villager_death(16)
assert falling["metadata"]["entity"] == "unit-death-villager"
assert corpse["metadata"]["entity"] == "unit-death-villager"
assert falling["metadata"]["sprite_frames"] == [
    {"resource_id": 1476, "frame": 4, "palette_player": 2}
]
assert corpse["metadata"]["sprite_frames"] == [
    {"resource_id": 1476, "frame": 14, "palette_player": 2}
]
assert (falling["x"], falling["y"]) == (310, 95)
assert (corpse["x"], corpse["y"]) == (296, 100)
assert struct.unpack_from(
    "<HH", (root / "tick-10-1" / falling["sprite"]).read_bytes(), 12
) == (25, 46)
assert struct.unpack_from(
    "<HH", (root / "tick-16-1" / corpse["sprite"]).read_bytes(), 12
) == (48, 31)
assert not any(case["metadata"]["entity"].startswith("building-rubble")
               and case["metadata"]["entity_id"] == 2
               for tick in (10, 16) for case in cases(tick))
PY
