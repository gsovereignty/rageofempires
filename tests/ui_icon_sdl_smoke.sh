#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-icons-sdl.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT
env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_COMMAND_PANEL=building \
    AOE_SELECTION_PROOF=1 \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    "AOE_SCREENSHOT_PATH=$smoke_dir/icons.bmp" \
    "$app_path" >"$smoke_dir/run.log" 2>&1
test -s "$smoke_dir/icons.bmp"
