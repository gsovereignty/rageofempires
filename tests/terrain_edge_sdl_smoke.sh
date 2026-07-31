#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-terrain-edge.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

frame="$smoke_dir/terrain-edge.bmp"
env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_DISABLE_LEGACY_ASSETS=1 \
    AOE_AUDIT_ANY_MAP_SIZE=1 \
    AOE_FOG=0 \
    AOE_WINDOW_SIZE=800x600 \
    AOE_CAMERA_TILE=6,4 \
    "AOE_SCENARIO_PATH=$script_dir/../resources/water-render-audit.scenario" \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    "AOE_SCREENSHOT_PATH=$frame" \
    "$app_path" >"$smoke_dir/run.log" 2>&1

test -s "$frame"
test "$(od -An -tu2 -j0 -N2 "$frame" | tr -d ' ')" = 19778
