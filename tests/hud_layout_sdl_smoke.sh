#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-hud-layout.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local size=$1
    local panel=$2
    local output="$smoke_dir/${size}-${panel}.bmp"
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        "AOE_WINDOW_SIZE=$size" \
        "AOE_COMMAND_PANEL=$panel" \
        AOE_SELECTION_PROOF=1 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$output" \
        "$app_path" >"$smoke_dir/${size}-${panel}.log" 2>&1
    test -s "$output"
    test "$(od -An -tu2 -j0 -N2 "$output" | tr -d ' ')" = 19778
}

# Physical-window scaling captures. Logical HUD remains 1280x720 because no
# executable evidence maps widths to original layout classes.
for size in 1280x1024 1024x768 640x480; do
    capture "$size" unit
    capture "$size" building
    ! cmp -s "$smoke_dir/${size}-unit.bmp" \
        "$smoke_dir/${size}-building.bmp"
done
