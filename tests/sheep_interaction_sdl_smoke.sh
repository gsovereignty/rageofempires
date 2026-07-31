#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-sheep-click.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

for mode in select gather; do
    frame="$smoke_dir/$mode.bmp"
    log="$smoke_dir/$mode.log"
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_DISABLE_LEGACY_ASSETS=1 \
        AOE_AUDIT_ANY_MAP_SIZE=1 \
        AOE_WINDOW_SIZE=800x600 \
        "AOE_SCENARIO_PATH=$script_dir/../resources/sheep-gather-audit.scenario" \
        "AOE_SHEEP_CLICK_PROOF=$mode" \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$frame" \
        "$app_path" >"$log" 2>&1
    test -s "$frame"
    grep -F "sheep click proof passed: $mode" "$log"
done
