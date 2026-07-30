#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/aoe_reconstruction" >&2
    exit 2
fi

executable=$1
base=$(dirname "$executable")
game_data="$base/game_data/Data"

test -f "$game_data/graphics.drs"
test -f "$game_data/interfac.drs"
test -f "$game_data/empires2_x1_p1.dat"
test -f "$game_data/pal_2.pal"

smoke_dir=$(mktemp -d)
frame="$smoke_dir/frame.bmp"
log="$smoke_dir/app.log"

SDL_VIDEO_DRIVER=dummy \
AOE_WINDOW_SIZE=800x600 \
AOE_SCREENSHOT_PATH="$frame" \
AOE_EXIT_AFTER_SCREENSHOT=1 \
"$executable" >"$log" 2>&1 &
pid=$!

finished=0
for attempt in $(seq 1 80); do
    if ! kill -0 "$pid" 2>/dev/null; then
        finished=1
        break
    fi
    sleep 0.25
done
if [ "$finished" -ne 1 ]; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
    echo "packaged sprite smoke timed out" >&2
    exit 1
fi
wait "$pid"

test -s "$frame"
grep -F "using optional original sprites from $game_data" "$log"
grep -F "using packaged frontend background from" "$log"
grep -F "using packaged scenario background from" "$log"
grep -F "using packaged campaign SLP background from" "$log"
grep -F "using packaged legacy localization:" "$log"
if grep -Fq "optional original sprites unavailable" "$log"; then
    echo "packaged sprite loader reported archive failure" >&2
    exit 1
fi

echo "packaged sprite smoke passed"
