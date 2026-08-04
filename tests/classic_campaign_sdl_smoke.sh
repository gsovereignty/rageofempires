#!/bin/sh
set -eu

game=$1
fixture_writer=$2
temporary=$(mktemp -d "${TMPDIR:-/tmp}/aoe-classic-campaign.XXXXXX")
trap 'rm -rf "$temporary"' EXIT HUP INT TERM

campaign="$temporary/fixture.cpx2"
"$fixture_writer" --write-cpx2 "$campaign"

if ! AOE_CAMPAIGN="$campaign" \
    AOE_EXIT_AFTER_SCREENSHOT=1 \
    AOE_SCREENSHOT_PATH="$temporary/campaign.bmp" \
    SDL_RENDER_DRIVER=software \
    SDL_VIDEODRIVER=dummy \
    "$game" >"$temporary/game.log" 2>&1; then
    tail -20 "$temporary/game.log" >&2
    exit 1
fi

if grep -Fq "Fatal error:" "$temporary/game.log"; then
    tail -20 "$temporary/game.log" >&2
    exit 1
fi

echo "classic campaign SDL smoke passed"
