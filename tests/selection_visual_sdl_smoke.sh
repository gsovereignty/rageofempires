#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-selection-visual.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local panel=$1
    local output=$2
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        "AOE_COMMAND_PANEL=$panel" \
        AOE_SELECTION_PROOF=1 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$output" \
        "$app_path" >"$smoke_dir/$panel.log" 2>&1
    test -s "$output"
}

capture unit "$smoke_dir/selected-unit.bmp"
capture building "$smoke_dir/selected-building.bmp"
