#!/usr/bin/env bash
set -euo pipefail

app_path=$1
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-signal-sdl.XXXXXX")
host_pid=
join_pid=
cleanup() {
    status=$?
    test -z "$host_pid" || kill "$host_pid" 2>/dev/null || true
    test -z "$join_pid" || kill "$join_pid" 2>/dev/null || true
    if [[ "$status" -ne 0 ]]; then
        test ! -f "$smoke_dir/host.log" || {
            echo "host log:"
            cat "$smoke_dir/host.log"
        }
        test ! -f "$smoke_dir/join.log" || {
            echo "join log:"
            cat "$smoke_dir/join.log"
        }
        test ! -f "$smoke_dir/host.state" ||
            cat "$smoke_dir/host.state"
        test ! -f "$smoke_dir/join.state" ||
            cat "$smoke_dir/join.state"
    fi
    rm -rf "$smoke_dir"
    return "$status"
}
trap cleanup EXIT
port=$(python3 -c \
    'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')
common=(
    "AOE_MULTIPLAYER_PORT=$port"
    "AOE_MULTIPLAYER_AUTO_READY=1"
    "AOE_MULTIPLAYER_EXIT_TICK=8"
    "AOE_MULTIPLAYER_SCRIPT_CHECKPOINT=8"
    "AOE_MULTIPLAYER_CHECKPOINT_PATH=$smoke_dir/checkpoint.save"
    "AOE_MULTIPLAYER_ALLIED=1"
    "SDL_VIDEODRIVER=dummy"
    "SDL_AUDIODRIVER=dummy"
    "SDL_RENDER_DRIVER=software"
)
env "${common[@]}" \
    AOE_MULTIPLAYER=host \
    AOE_MULTIPLAYER_AUTO_START=1 \
    "AOE_MULTIPLAYER_SCRIPT_SIGNAL=3,7|4,7" \
    "AOE_MULTIPLAYER_STATE_PATH=$smoke_dir/host.state" \
    "$app_path" >"$smoke_dir/host.log" 2>&1 &
host_pid=$!
sleep 0.1
env "${common[@]}" \
    AOE_MULTIPLAYER=join \
    "AOE_MULTIPLAYER_SCRIPT_SIGNAL=5,7" \
    "AOE_MULTIPLAYER_STATE_PATH=$smoke_dir/join.state" \
    "$app_path" >"$smoke_dir/join.log" 2>&1 &
join_pid=$!
for _ in {1..150}; do
    if [[ -s "$smoke_dir/host.state" &&
          -s "$smoke_dir/join.state" ]]; then
        break
    fi
    sleep 0.1
done
test -s "$smoke_dir/host.state"
test -s "$smoke_dir/join.state"
wait "$host_pid"
wait "$join_pid"
host_pid=
join_pid=
test "$(awk '$1 == "signal" { print $2 }' "$smoke_dir/host.state")" = \
    "$(printf '1\n2\n3')"
test "$(awk '$1 == "signal" { print $2 }' "$smoke_dir/join.state")" = \
    "$(printf '1\n2\n3')"
grep -q '^signal .* blue allies 3 7$' "$smoke_dir/join.state"
grep -q '^signal .* blue allies 4 7$' "$smoke_dir/join.state"
grep -q '^signal .* red allies 5 7$' "$smoke_dir/host.state"
test "$(awk '$1 == "hash" { print $2 }' "$smoke_dir/host.state")" = \
    "$(awk '$1 == "hash" { print $2 }' "$smoke_dir/join.state")"
