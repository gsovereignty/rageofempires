#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-audio-playback.XXXXXX")
cleanup() {
    status=$?
    if [ "$status" -ne 0 ] && [ -f "$smoke_dir/audio.log" ]; then
        sed -n '1,80p' "$smoke_dir/audio.log" >&2
    fi
    rm -rf "$smoke_dir"
    exit "$status"
}
trap cleanup EXIT

env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_AUDIO_TRACE=1 \
    AOE_AUDIO_PROOF_CONTEXT=gameplay \
    AOE_AUDIO_PROOF_TAUNT=1 \
    AOE_AUDIO_PROOF_NARRATION=A1AA.mp3 \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    "AOE_SCREENSHOT_PATH=$smoke_dir/audio.bmp" \
    "$app_path" >"$smoke_dir/audio.log" 2>&1

test -s "$smoke_dir/audio.bmp"
grep -q '^Audio music open.mp3$' "$smoke_dir/audio.log"
grep -q '^Audio music xmusic1.mp3$' "$smoke_dir/audio.log"
grep -q '^Audio ambience ' "$smoke_dir/audio.log"
grep -q '^Audio loose effect 01 Yes.mp3$' "$smoke_dir/audio.log"
grep -q '^Audio loose effect A1AA.mp3$' "$smoke_dir/audio.log"
! grep -Eq '^Audio music (Countdwn|lost|won1|won2|credits|xcredits)\\.mp3$' \
    "$smoke_dir/audio.log"
! grep -q '^Music unavailable:' "$smoke_dir/audio.log"

env \
    SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy \
    SDL_RENDER_DRIVER=software \
    AOE_AUDIO_TRACE=1 \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    "AOE_SCREENSHOT_PATH=$smoke_dir/main-menu.bmp" \
    "$app_path" >"$smoke_dir/main-menu-audio.log" 2>&1

test -s "$smoke_dir/main-menu.bmp"
grep -q '^Audio music open.mp3$' "$smoke_dir/main-menu-audio.log"
! grep -Eq '^Audio music (Random|xtown)\\.mp3$' \
    "$smoke_dir/main-menu-audio.log"
