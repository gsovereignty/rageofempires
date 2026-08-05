#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-tower-damage.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

civilizations=${AOE_TOWER_DAMAGE_AUDIT_CIVILIZATIONS:-"britons franks celts spanish goths teutons vikings huns japanese chinese mongols koreans byzantines persians saracens turks aztecs mayans"}
for civilization in $civilizations; do
    for tier in watch_tower guard_tower keep; do
        scenario="$smoke_dir/$civilization-$tier.scenario"
        guard_replacement="# guard technology"
        keep_replacement="# keep technology"
        if [ "$tier" = guard_tower ] || [ "$tier" = keep ]; then
            guard_replacement="technology blue guard_tower"
        fi
        if [ "$tier" = keep ]; then
            keep_replacement="technology blue keep"
        fi
        sed \
            -e "s/civilization blue generic/civilization blue $civilization/" \
            -e "s/# guard technology/$guard_replacement/" \
            -e "s/# keep technology/$keep_replacement/" \
            "$script_dir/../resources/tower-damage-state-audit.scenario" \
            >"$scenario"
        for damage in 0 25 26 50 51 75 76; do
            env -u AOE_DISABLE_LEGACY_ASSETS \
                SDL_VIDEODRIVER=dummy \
                SDL_AUDIODRIVER=dummy \
                SDL_RENDER_DRIVER=software \
                AOE_MAIN_MENU=0 \
                AOE_AUDIT_ANY_MAP_SIZE=1 \
                AOE_FOG=0 \
                AOE_WINDOW_SIZE=800x600 \
                AOE_CAMERA_TILE=5,4 \
                "AOE_SCENARIO_PATH=$scenario" \
                "AOE_TOWER_DAMAGE_AUDIT_PERCENT=$damage" \
                AOE_TOWER_DAMAGE_AUDIT_SAMPLE_MS=250 \
                AOE_EXIT_AFTER_SCREENSHOT=1 \
                "AOE_SCREENSHOT_PATH=$smoke_dir/$civilization-$tier-$damage.bmp" \
                "$app_path" >"$smoke_dir/$civilization-$tier-$damage.log" 2>&1
        done
    done
done

python3 - "$smoke_dir" "$civilizations" <<'PY'
import hashlib
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
civilizations = sys.argv[2].split()
tiers = ("watch_tower", "guard_tower", "keep")
for civilization in civilizations:
    for tier in tiers:
        digests = {
            damage: hashlib.sha256(
                (root / f"{civilization}-{tier}-{damage}.bmp").read_bytes()
            ).hexdigest()
            for damage in (0, 25, 26, 50, 51, 75, 76)
        }
        if digests[0] != digests[25]:
            raise SystemExit(f"{civilization}/{tier}: 25 equality changed")
        if digests[26] != digests[50]:
            raise SystemExit(f"{civilization}/{tier}: 50 equality changed")
        if digests[51] != digests[75]:
            raise SystemExit(f"{civilization}/{tier}: 75 equality changed")
        visible = {digests[0], digests[26], digests[51], digests[76]}
        if len(visible) < 2:
            raise SystemExit(f"{civilization}/{tier}: no visible damage")
PY
