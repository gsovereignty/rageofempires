#!/usr/bin/env bash
set -euo pipefail

app_path=$1
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-gameplay-performance.XXXXXX")
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
    "AOE_GAMEPLAY_BENCHMARK_PATH=$test_dir/report.json" \
    "$app_path" >"$test_dir/run.log" 2>&1

test -s "$test_dir/report.json"
report=$(cat "$test_dir/report.json")
frames=$(printf '%s\n' "$report" |
    sed -E 's/.*"frames":([0-9]+).*/\1/')
frame_p95_ms=$(printf '%s\n' "$report" |
    sed -E 's/.*"frame_p95_ms":([0-9.]+).*/\1/')
frame_max_ms=$(printf '%s\n' "$report" |
    sed -E 's/.*"frame_max_ms":([0-9.]+).*/\1/')
max_tiles=$(printf '%s\n' "$report" |
    sed -E 's/.*"max_tiles":([0-9]+).*/\1/')
command_ms=$(printf '%s\n' "$report" |
    sed -E 's/.*"command_ms":([0-9.]+).*/\1/')
commanded_units=$(printf '%s\n' "$report" |
    sed -E 's/.*"commanded_units":([0-9]+).*/\1/')
max_moving_units=$(printf '%s\n' "$report" |
    sed -E 's/.*"max_moving_units":([0-9]+).*/\1/')

test "$frames" -eq 120
# Deterministic maximum-map fixture contains four local land units. Requiring
# exact workload prevents map-generation changes from making this regression
# test pass without exercising group formation pathfinding.
test "$commanded_units" -eq 4
test "$max_moving_units" -ge "$commanded_units"
test "$max_tiles" -lt 2500
# Original repeated entity/candidate scans took about 741 ms here. Keep ample
# CI headroom over optimized single-digit milliseconds while catching relapse.
awk -v value="$command_ms" 'BEGIN { exit !(value < 100.0) }'
awk -v value="$frame_p95_ms" 'BEGIN { exit !(value < 100.0) }'
awk -v value="$frame_max_ms" 'BEGIN { exit !(value < 250.0) }'

printf 'gameplay benchmark: %s\n' "$report"
