#!/usr/bin/env bash
set -euo pipefail

app_path=$1
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-wall-damage.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

civilizations="britons franks celts spanish goths teutons vikings huns japanese chinese mongols koreans byzantines persians saracens turks aztecs mayans"
for civilization in $civilizations; do
    scenario="$smoke_dir/$civilization.scenario"
    sed "s/civilization blue generic/civilization blue $civilization/" \
        "$script_dir/../resources/wall-damage-state-audit.scenario" \
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
            AOE_CAMERA_TILE=6,5 \
            "AOE_SCENARIO_PATH=$scenario" \
            "AOE_WALL_DAMAGE_AUDIT_PERCENT=$damage" \
            AOE_EXIT_AFTER_SCREENSHOT=1 \
            "AOE_SCREENSHOT_PATH=$smoke_dir/$civilization-$damage.bmp" \
            "$app_path" >"$smoke_dir/$civilization-$damage.log" 2>&1
    done
done

python3 - "$smoke_dir" <<'PY'
import hashlib
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
civilizations = "britons franks celts spanish goths teutons vikings huns japanese chinese mongols koreans byzantines persians saracens turks aztecs mayans".split()
for civilization in civilizations:
    digests = {
        damage: hashlib.sha256(
            (root / f"{civilization}-{damage}.bmp").read_bytes()
        ).hexdigest()
        for damage in (0, 25, 26, 50, 51, 75, 76)
    }
    if digests[0] != digests[25]:
        raise SystemExit(f"{civilization}: equality at 25 selected damage")
    if digests[26] != digests[50]:
        raise SystemExit(f"{civilization}: equality at 50 changed damage")
    if digests[51] != digests[75]:
        raise SystemExit(f"{civilization}: equality at 75 changed damage")
    if len({digests[0], digests[26], digests[51], digests[76]}) != 4:
        raise SystemExit(f"{civilization}: missing visible replacement stage")
PY
