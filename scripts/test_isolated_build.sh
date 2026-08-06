#!/bin/sh
set -eu

source_root=${1:-.}
source_root=$(cd "$source_root" && pwd)
isolation_root=$(mktemp -d)
isolated_source="$isolation_root/reconstruction"

mkdir "$isolated_source"
rsync -a \
    --exclude '/.git/' \
    --exclude '/build/' \
    --exclude '/build-*/' \
    --exclude '/artifacts/' \
    --exclude '/.codebase-memory/' \
    "$source_root/" "$isolated_source/"

cmake -S "$isolated_source" -B "$isolated_source/build"
cmake --build "$isolated_source/build" -j4 \
    --target aoe_deploy_runtime_resources

frame="$isolation_root/frame.bmp"
log="$isolation_root/runtime.log"
SDL_VIDEO_DRIVER=dummy \
AOE_WINDOW_SIZE=800x600 \
AOE_SCREENSHOT_PATH="$frame" \
AOE_EXIT_AFTER_SCREENSHOT=1 \
"$isolated_source/build/aoe_reconstruction" >"$log" 2>&1

game_data=$(cd "$isolated_source/build/game_data/Data" && pwd -P)
game_data_root=$(cd "$isolated_source/build/game_data" && pwd -P)
test -s "$frame"
grep -F "using optional original sprites from $game_data" "$log"
grep -F "using packaged HD terrain textures from $game_data_root" "$log"
python3 "$isolated_source/scripts/verify_resource_manifest.py" \
    "$isolated_source/build/resource-manifest.json" \
    "$isolated_source/build"
if grep -Fq "$source_root" "$log"; then
    echo "isolated runtime referenced source workspace" >&2
    exit 1
fi

python3 "$isolated_source/scripts/check_self_containment.py" \
    "$isolated_source"
echo "isolated build and packaged sprite runtime passed: $isolation_root"
