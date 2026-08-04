#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-market-standing.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

for tick in 0 12 20 24 40 64; do
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        AOE_AUDIT_ANY_MAP_SIZE=1 \
        AOE_FOG=0 \
        AOE_WINDOW_SIZE=1280x720 \
        AOE_CAMERA_TILE=8,5 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/market-standing-regression.scenario" \
        "AOE_SCREENSHOT_ADVANCE_TICKS=$tick" \
        "AOE_SCREENSHOT_TICK=$tick" \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/tick-$tick.bmp" \
        "$app_path" >"$smoke_dir/tick-$tick.log" 2>&1
done

for tick in 12 20 24 40 64; do
    cmp "$smoke_dir/tick-0.bmp" "$smoke_dir/tick-$tick.bmp"
done
