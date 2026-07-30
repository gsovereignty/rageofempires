#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-technology-tree.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

language_file="$smoke_dir/long.lang"
cat >"$language_file" <<'EOF'
aoe-language 1
locale "zz"
string "technology_tree.title" "REPRESENTATIVE EXTENDED CIVILIZATION TECHNOLOGY CATALOG"
string "technology_tree.help" "Q/E CHANGES CIVILIZATION  ARROWS MOVE VISIBLE FOCUS  TAB SELECTS NEXT CATALOG ENTRY"
string "technology_tree.missing_evidence" "MISSING EVIDENCE REMAINS EXPLICIT FOR ORIGINAL ICONS FRAMING NAVIGATION AND FONT METRICS"
EOF

capture() {
    local name=$1
    shift
    env \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_TECH_TREE=1 \
        AOE_EXIT_AFTER_SCREENSHOT=1 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/$name.bmp" \
        "$@" \
        "$app_path" >"$smoke_dir/$name.log" 2>&1
    test -s "$smoke_dir/$name.bmp"
    test "$(od -An -tu2 -j0 -N2 "$smoke_dir/$name.bmp" | tr -d ' ')" = 19778
}

capture english env
capture long env AOE_LOCALE=zz "AOE_LANGUAGE_FILE=$language_file"
! cmp -s "$smoke_dir/english.bmp" "$smoke_dir/long.bmp"
