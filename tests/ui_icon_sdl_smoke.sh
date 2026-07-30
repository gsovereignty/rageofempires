#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-icons-sdl.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local panel=$1
    local output="$smoke_dir/$panel.bmp"
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

capture villager
capture scout
capture building

! cmp -s "$smoke_dir/villager.bmp" "$smoke_dir/scout.bmp"
! cmp -s "$smoke_dir/villager.bmp" "$smoke_dir/building.bmp"
! cmp -s "$smoke_dir/scout.bmp" "$smoke_dir/building.bmp"
