#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-random-map-sdl.XXXXXX")
cleanup() {
    status=$?
    if [[ "$status" -ne 0 ]]; then
        test ! -f "$smoke_dir/run.log" || cat "$smoke_dir/run.log"
    fi
    rm -rf "$smoke_dir"
    return "$status"
}
trap cleanup EXIT

for capture in first second; do
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_RANDOM_MAP_SETUP=1 \
        AOE_RANDOM_MAP_SEED=424242 \
        "AOE_MAP_DIMENSION_PATH=$smoke_dir/$capture.dimensions" \
        AOE_SCREENSHOT_TICK=0 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$capture.bmp" \
        "$app_path" >"$smoke_dir/run.log" 2>&1
    test -s "$smoke_dir/$capture.bmp"
    test "$(cat "$smoke_dir/$capture.dimensions")" = \
"map 200 200
tiles 40000
preview 200 200
preview_tiles 40000"
done

cmp "$smoke_dir/first.bmp" "$smoke_dir/second.bmp"
cmp "$smoke_dir/first.dimensions" "$smoke_dir/second.dimensions"
