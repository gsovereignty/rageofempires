#!/usr/bin/env bash
set -euo pipefail

app_path=$1
asset_root=$2
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-audio-playback.XXXXXX")
trap 'rm -rf "$smoke_dir"' EXIT

env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_AUDIO_TRACE=1 \
    AOE_AUDIO_PROOF_CONTEXT=gameplay \
    "AOE_ASSET_ROOT=$asset_root" \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    "AOE_SCREENSHOT_PATH=$smoke_dir/audio.bmp" \
    "$app_path" >"$smoke_dir/audio.log" 2>&1

test -s "$smoke_dir/audio.bmp"
grep -q '^Audio music xopen.mp3$' "$smoke_dir/audio.log"
grep -q '^Audio music xmusic1.mp3$' "$smoke_dir/audio.log"
grep -q '^Audio ambience ' "$smoke_dir/audio.log"
! grep -Eq '^Audio music (Countdwn|lost|won1|won2|credits|xcredits)\\.mp3$' \
    "$smoke_dir/audio.log"
! grep -q '^Music unavailable:' "$smoke_dir/audio.log"
