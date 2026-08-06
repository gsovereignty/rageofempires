#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-icons-sdl.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

capture() {
    local panel=$1
    local output="$smoke_dir/$panel.bmp"
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        "AOE_COMMAND_PANEL=$panel" \
        AOE_SELECTION_PROOF=1 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$output" \
        "$app_path" >"$smoke_dir/$panel.log" 2>&1
    test -s "$output"
}

capture villager
capture scout
capture building

for run in 1 2; do
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MAIN_MENU=0 \
        "AOE_SCENARIO_PATH=$(dirname "$0")/../resources/trade-cart-selection-regression.scenario" \
        AOE_COMMAND_PANEL=trade_cart \
        AOE_SELECTION_PROOF=1 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/trade-cart-$run.bmp" \
        "$app_path" >"$smoke_dir/trade-cart-$run.log" 2>&1
    test -s "$smoke_dir/trade-cart-$run.bmp"
done

! cmp -s "$smoke_dir/villager.bmp" "$smoke_dir/scout.bmp"
! cmp -s "$smoke_dir/villager.bmp" "$smoke_dir/building.bmp"
! cmp -s "$smoke_dir/scout.bmp" "$smoke_dir/building.bmp"
cmp "$smoke_dir/trade-cart-1.bmp" "$smoke_dir/trade-cart-2.bmp"
! cmp -s "$smoke_dir/trade-cart-1.bmp" "$smoke_dir/villager.bmp"

naval_scenario="$(dirname "$0")/../resources/naval-selection-portrait-regression.scenario"
for kind in galley war_galley galleon transport_ship; do
    for run in 1 2; do
        env \
            SDL_VIDEODRIVER=dummy \
            SDL_AUDIODRIVER=dummy \
            SDL_RENDER_DRIVER=software \
            AOE_MAIN_MENU=0 \
            "AOE_SCENARIO_PATH=$naval_scenario" \
            "AOE_COMMAND_PANEL=$kind" \
            AOE_EXIT_AFTER_SCREENSHOT=1 \
            "AOE_SCREENSHOT_PATH=$smoke_dir/$kind-$run.bmp" \
            "$app_path" >"$smoke_dir/$kind-$run.log" 2>&1
        test -s "$smoke_dir/$kind-$run.bmp"
    done
done
python3 - \
    "$smoke_dir/galley-1.bmp" "$smoke_dir/galley-2.bmp" \
    "$smoke_dir/war_galley-1.bmp" "$smoke_dir/war_galley-2.bmp" \
    "$smoke_dir/galleon-1.bmp" "$smoke_dir/galleon-2.bmp" \
    "$smoke_dir/transport_ship-1.bmp" \
    "$smoke_dir/transport_ship-2.bmp" <<'PY'
import struct
import sys

def portrait(path):
    data = open(path, "rb").read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    assert (width, height) == (1280, 720)
    stride = (width * 3 + 3) & ~3
    return b"".join(
        data[
            offset + (height - 1 - y) * stride + x * 3:
            offset + (height - 1 - y) * stride + x * 3 + 3
        ]
        for y in range(562, 648)
        for x in range(287, 373)
    )

for first, second in zip(sys.argv[1::2], sys.argv[2::2]):
    assert portrait(first) == portrait(second), (
        f"nondeterministic naval portrait: {first} {second}"
    )
PY

for scenario in guard-tower-render-audit keep-render-audit fortified-wall-render-audit; do
    for run in 1 2; do
        env \
            SDL_VIDEODRIVER=dummy \
            SDL_AUDIODRIVER=dummy \
            SDL_RENDER_DRIVER=software \
            AOE_MAIN_MENU=0 \
            "AOE_SCENARIO_PATH=$(dirname "$0")/../resources/$scenario.scenario" \
            AOE_COMMAND_PANEL=building \
            AOE_SELECTION_PROOF=1 \
            AOE_EXIT_AFTER_SCREENSHOT=1 \
            "AOE_SCREENSHOT_PATH=$smoke_dir/$scenario-$run.bmp" \
            "$app_path" >"$smoke_dir/$scenario-$run.log" 2>&1
        test -s "$smoke_dir/$scenario-$run.bmp"
    done
    cmp "$smoke_dir/$scenario-1.bmp" "$smoke_dir/$scenario-2.bmp"
done
! cmp -s "$smoke_dir/guard-tower-render-audit-1.bmp" \
    "$smoke_dir/keep-render-audit-1.bmp"
! cmp -s "$smoke_dir/guard-tower-render-audit-1.bmp" \
    "$smoke_dir/fortified-wall-render-audit-1.bmp"

# Opt-in archive-backed proof: exact 50730 artwork has rich color detail.
# Default CTest deliberately disables optional legacy assets.
if [[ "${AOE_UI_ICON_VISUAL_PROOF:-0}" != 0 ]]; then
python3 - \
    "$smoke_dir/trade-cart-1.bmp" \
    "$smoke_dir/galley-1.bmp" \
    "$smoke_dir/war_galley-1.bmp" \
    "$smoke_dir/galleon-1.bmp" \
    "$smoke_dir/transport_ship-1.bmp" <<'PY'
import struct
import sys

portraits = []
for path in sys.argv[1:]:
    data = open(path, "rb").read()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    assert (width, height) == (1280, 720)
    stride = (width * 3 + 3) & ~3
    colors = set()
    portrait = bytearray()
    for y in range(562, 648):
        row = offset + (height - 1 - y) * stride
        for x in range(287, 373):
            pixel = row + x * 3
            value = data[pixel:pixel + 3]
            colors.add(value)
            portrait.extend(value)
    assert len(colors) >= 64, (
        f"{path} portrait reverted to flat fallback: {len(colors)} colors"
    )
    portraits.append(bytes(portrait))
assert len(set(portraits[1:])) == 4, "naval portraits are not distinct"
PY
fi
