#!/usr/bin/env bash
set -euo pipefail

app_path=$1
fixture=$2
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-editor.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_EDITOR=1 \
    "AOE_EDITOR_INPUT=$fixture" \
    "AOE_EDITOR_PATH=$smoke_dir/roundtrip.scenario" \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    "AOE_SCREENSHOT_PATH=$smoke_dir/editor.bmp" \
    "$app_path" >"$smoke_dir/editor.log" 2>&1

test -s "$smoke_dir/editor.bmp"
test "$(od -An -tu2 -j0 -N2 "$smoke_dir/editor.bmp" | tr -d ' ')" = 19778
