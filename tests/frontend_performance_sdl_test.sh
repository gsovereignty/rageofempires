#!/usr/bin/env bash
set -euo pipefail

app_path=$1
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-frontend-performance.XXXXXX")
cleanup() {
    status=$?
    if [[ "$status" -ne 0 ]]; then
        test ! -f "$test_dir/run.log" || cat "$test_dir/run.log"
        test ! -f "$test_dir/report.json" || cat "$test_dir/report.json"
    fi
    rm -rf "$test_dir"
    return "$status"
}
trap cleanup EXIT

env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_MAIN_MENU=1 \
    "AOE_GAMEPLAY_BENCHMARK_PATH=$test_dir/report.json" \
    "$app_path" >"$test_dir/run.log" 2>&1

test -s "$test_dir/report.json"
report=$(cat "$test_dir/report.json")
frames=$(printf '%s\n' "$report" |
    sed -E 's/.*"frames":([0-9]+).*/\1/')
frame_p95_ms=$(printf '%s\n' "$report" |
    sed -E 's/.*"frame_p95_ms":([0-9.]+).*/\1/')
max_tiles=$(printf '%s\n' "$report" |
    sed -E 's/.*"max_tiles":([0-9]+).*/\1/')

test "$frames" -eq 120
test "$max_tiles" -eq 0
awk -v value="$frame_p95_ms" 'BEGIN { exit !(value < 20.0) }'

printf 'frontend benchmark: %s\n' "$report"
