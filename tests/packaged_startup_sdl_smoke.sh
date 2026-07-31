#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-packaged-startup.XXXXXX")
cleanup() {
    status=$?
    if [ "$status" -ne 0 ]; then
        for log in "$smoke_dir"/*.log; do
            test -f "$log" && sed -n '1,80p' "$log" >&2
        done
    fi
    rm -rf "$smoke_dir"
    exit "$status"
}
trap cleanup EXIT

run_capture() {
    output=$1
    shift
    (
        cd "$smoke_dir"
        env \
            SDL_VIDEODRIVER=dummy \
            SDL_AUDIODRIVER=dummy \
            SDL_RENDER_DRIVER=software \
            AOE_MUTE=1 \
            AOE_EXIT_AFTER_SCREENSHOT=1 \
            "AOE_SCREENSHOT_PATH=$smoke_dir/$output.bmp" \
            "$@" "$app_path" >"$smoke_dir/$output.log" 2>&1
    )
    test -s "$smoke_dir/$output.bmp"
}

# Default packaged launch must equal explicit main-menu launch, even from an
# unrelated working directory. No startup-mode variable is used in first run.
run_capture default env -u AOE_MAIN_MENU
run_capture explicit env AOE_MAIN_MENU=1
cmp "$smoke_dir/default.bmp" "$smoke_dir/explicit.bmp"

echo "packaged startup main-menu smoke passed"
