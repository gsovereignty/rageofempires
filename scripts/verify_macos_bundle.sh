#!/bin/sh
set -eu

if [ "$#" -eq 1 ]; then
    app=$1
else
    app="build-release/AoE Archaeology.app"
fi

executable="$app/Contents/MacOS/AoE Archaeology"
framework="$app/Contents/Frameworks/libSDL3.0.dylib"
scenario="$app/Contents/Resources/demo.scenario"
resource_manifest="$app/Contents/Resources/resource-manifest.json"
resource_tree="$app/Contents/Resources/resources"
game_data="$app/Contents/Resources/game_data/Data"

require_universal_2() {
    binary=$1
    architectures=$(lipo -archs "$binary")
    case " $architectures " in
        *" arm64 "*) ;;
        *)
            echo "$binary is missing its arm64 slice" >&2
            exit 1
            ;;
    esac
    case " $architectures " in
        *" x86_64 "*) ;;
        *)
            echo "$binary is missing its x86_64 slice" >&2
            exit 1
            ;;
    esac
}

require_macos_11_per_slice() {
    binary=$1
    minimum_count=$(otool -l "$binary" |
        awk '$1 == "minos" { count++ } END { print count + 0 }')
    test "$minimum_count" -eq 2
    if ! otool -l "$binary" |
        awk '$1 == "minos" && $2 != "11.0" { exit 1 }'; then
        echo "$binary contains a slice targeting newer than macOS 11.0" >&2
        exit 1
    fi
}

test -x "$executable"
test -f "$framework"
test -f "$scenario"
test -f "$resource_manifest"
test -d "$resource_tree"
test -f "$game_data/graphics.drs"
test -f "$game_data/interfac.drs"
test -f "$game_data/empires2_x1_p1.dat"
test -f "$game_data/pal_2.pal"
test -f "$game_data/sounds.drs"
test -f "$game_data/sounds_x1.drs"
test -f "$app/Contents/Resources/game_data/Sound/music/xmusic1.mp3"
test -f "$app/Contents/Resources/game_data/Sound/stream/xopen.mp3"
test -f "$app/Contents/Resources/game_data/Sound/terrain/Wave1.wav"
test -f "$app/Contents/Resources/game_data/Terrain/Textures/g_grs_00_COLOR.png"
python3 "$(dirname "$0")/verify_resource_manifest.py" \
    "$resource_manifest" "$app/Contents/Resources"
plutil -lint "$app/Contents/Info.plist"
test "$(plutil -extract LSMinimumSystemVersion raw \
    "$app/Contents/Info.plist")" = "11.0"

if otool -L "$executable" | grep -q '/opt/homebrew'; then
    echo "Bundle still links Homebrew directly" >&2
    exit 1
fi

otool -L "$executable" | grep -q '@rpath/libSDL3.0.dylib'
codesign --verify --deep --strict "$app"
require_universal_2 "$executable"
require_universal_2 "$framework"
require_macos_11_per_slice "$executable"
require_macos_11_per_slice "$framework"

smoke_dir=$(mktemp -d)
smoke_frame="$smoke_dir/frame.bmp"
smoke_log="$smoke_dir/app.log"
cleanup_smoke() {
    rm -f "$smoke_frame" "$smoke_log"
    rmdir "$smoke_dir" 2>/dev/null || true
}
trap cleanup_smoke EXIT HUP INT TERM
SDL_VIDEO_DRIVER=dummy \
AOE_WINDOW_SIZE=800x600 \
AOE_SCREENSHOT_PATH="$smoke_frame" \
AOE_EXIT_AFTER_SCREENSHOT=1 \
"$executable" >"$smoke_log" 2>&1 &
smoke_pid=$!
smoke_finished=0
for attempt in $(seq 1 80); do
    if ! kill -0 "$smoke_pid" 2>/dev/null; then
        smoke_finished=1
        break
    fi
    sleep 0.25
done
if [ "$smoke_finished" -ne 1 ]; then
    kill "$smoke_pid" 2>/dev/null || true
    wait "$smoke_pid" 2>/dev/null || true
    echo "Bundled app did not complete runtime smoke test" >&2
    sed -n '1,20p' "$smoke_log" >&2
    exit 1
fi
if ! wait "$smoke_pid"; then
    echo "Bundled app failed runtime smoke test" >&2
    sed -n '1,20p' "$smoke_log" >&2
    exit 1
fi
test -s "$smoke_frame"
sips -g pixelWidth -g pixelHeight "$smoke_frame" |
    grep -q 'pixelWidth: 800'
sips -g pixelWidth -g pixelHeight "$smoke_frame" |
    grep -q 'pixelHeight: 600'
cleanup_smoke
trap - EXIT HUP INT TERM

echo "macOS 11+ bundle is self-contained, signed, Universal 2, and runtime-smoked"
