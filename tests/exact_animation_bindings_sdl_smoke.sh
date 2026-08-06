#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-exact-animation.XXXXXX")
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
        AOE_CAMERA_TILE=4,3 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/animation-binding-audit.scenario" \
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

mkdir -p "$smoke_dir/build" "$smoke_dir/repair"
env -u AOE_DISABLE_LEGACY_ASSETS \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_MAIN_MENU=0 \
    AOE_AUDIT_ANY_MAP_SIZE=1 \
    AOE_FOG=0 \
    AOE_WINDOW_SIZE=1280x720 \
    AOE_CAMERA_TILE=4,3 \
    "AOE_SCENARIO_PATH=$script_dir/../resources/villager-build-animation-audit.scenario" \
    AOE_SCREENSHOT_SELECT_TILE=3,2 \
    AOE_SCREENSHOT_BUILD_OUTPOST=4,2 \
    AOE_SCREENSHOT_ADVANCE_TICKS=1 \
    AOE_SCREENSHOT_TICK=1 \
    "AOE_OVERLAP_CAPTURE_DIR=$smoke_dir/build" \
    AOE_OVERLAP_CAPTURE_TICK=1 \
    AOE_OVERLAP_CAPTURE_EXIT=1 \
    "$app_path" >"$smoke_dir/build.log" 2>&1
env -u AOE_DISABLE_LEGACY_ASSETS \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_MAIN_MENU=0 \
    AOE_AUDIT_ANY_MAP_SIZE=1 \
    AOE_FOG=0 \
    AOE_WINDOW_SIZE=1280x720 \
    AOE_CAMERA_TILE=4,3 \
    "AOE_SCENARIO_PATH=$script_dir/../resources/villager-repair-audit.scenario" \
    AOE_SCREENSHOT_SELECT_TILE=3,2 \
    AOE_SCREENSHOT_COMMAND_TILE=4,2 \
    AOE_SCREENSHOT_ADVANCE_TICKS=1 \
    AOE_SCREENSHOT_TICK=1 \
    "AOE_OVERLAP_CAPTURE_DIR=$smoke_dir/repair" \
    AOE_OVERLAP_CAPTURE_TICK=1 \
    AOE_OVERLAP_CAPTURE_EXIT=1 \
    "$app_path" >"$smoke_dir/repair.log" 2>&1

cmp "$smoke_dir/tick-0-1/actual.bmp" "$smoke_dir/tick-0-2/actual.bmp"
cmp "$smoke_dir/tick-1-1/actual.bmp" "$smoke_dir/tick-1-2/actual.bmp"

python3 - "$smoke_dir" <<'PY'
import json
from pathlib import Path
import sys

root = Path(sys.argv[1])

def unit(tick, kind, ownership):
    manifest = json.loads(
        (root / f"tick-{tick}-1/manifest.json").read_text()
    )
    matches = [
        case for case in manifest["cases"]
        if case["metadata"]["entity"] == f"unit-{kind}" and
           case["metadata"]["ownership"] == ownership
    ]
    assert len(matches) == 1
    return matches[0]

king_idle = unit(0, "king", 0)
king_move = unit(0, "king", 1)
woad_idle = unit(0, "woad_raider", 0)
woad_move = unit(0, "woad_raider", 1)

assert [frame["resource_id"] for frame in
        king_idle["metadata"]["sprite_frames"]] == [1767]
assert [frame["resource_id"] for frame in
        king_move["metadata"]["sprite_frames"]] == [1771]
assert [frame["resource_id"] for frame in
        woad_idle["metadata"]["sprite_frames"]] == [1598]
assert [frame["resource_id"] for frame in
        woad_move["metadata"]["sprite_frames"]] == [1602]
assert "blocked_reason" not in king_idle
assert "blocked_reason" not in king_move

def work(mode):
    manifest = json.loads((root / f"{mode}/manifest.json").read_text())
    matches = [
        case for case in manifest["cases"]
        if case["metadata"]["entity"] == "unit-villager" and
           case["metadata"]["ownership"] == 0
    ]
    assert len(matches) == 1
    return matches[0]

for mode in ("build", "repair"):
    case = work(mode)
    assert [frame["resource_id"] for frame in
            case["metadata"]["sprite_frames"]] == [1496]
    assert "blocked_reason" not in case
PY
