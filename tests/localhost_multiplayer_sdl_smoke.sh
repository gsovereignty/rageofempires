#!/usr/bin/env bash
set -euo pipefail

app_path=$1
if [[ -z "${AOE_SMOKE_ATTEMPT:-}" ]]; then
    for session_attempt in 1 2 3; do
        if AOE_SMOKE_ATTEMPT=$session_attempt \
            bash "$0" "$app_path"; then
            exit 0
        fi
        echo "multiplayer session attempt $session_attempt failed; retrying with fresh port/state" >&2
    done
    echo "multiplayer smoke failed after 3 complete session attempts" >&2
    exit 1
fi
smoke_dir=$(mktemp -d "${TMPDIR:-/tmp}/aoe-multiplayer-sdl.XXXXXX")
host_pid=
join_pid=
cleanup() {
    status=$?
    if [[ -n "$host_pid" ]]; then kill "$host_pid" 2>/dev/null || true; fi
    if [[ -n "$join_pid" ]]; then kill "$join_pid" 2>/dev/null || true; fi
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

port=
for attempt in 1 2 3; do
    port=$(python3 -c \
        'import socket; s=socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')
    attempt_dir="$smoke_dir/attempt-$attempt"
    mkdir -p "$attempt_dir"
    env \
        "AOE_MULTIPLAYER_PORT=$port" \
        AOE_MULTIPLAYER_AUTO_READY=1 \
        AOE_MULTIPLAYER_EXIT_TICK=12 \
        AOE_MULTIPLAYER_INPUT_DELAY=3 \
        AOE_MULTIPLAYER_SCRIPT_CHECKPOINT=12 \
        AOE_MULTIPLAYER_SCRIPT_CONTROL=1 \
        SDL_VIDEODRIVER=dummy \
        SDL_AUDIODRIVER=dummy \
        SDL_RENDER_DRIVER=software \
        AOE_MULTIPLAYER=host \
        AOE_MULTIPLAYER_AUTO_START=1 \
        AOE_MULTIPLAYER_SCRIPT_MOVE=1,3,7 \
        "AOE_MULTIPLAYER_SCRIPT_CHAT=all:Host hello|allies:Blue allies only" \
        "AOE_MULTIPLAYER_CHECKPOINT_PATH=$smoke_dir/checkpoint.save" \
        "AOE_MULTIPLAYER_STATE_PATH=$smoke_dir/host.state" \
        AOE_SCREENSHOT_TICK=12 \
        "AOE_SCREENSHOT_PATH=$smoke_dir/host.bmp" \
        "$app_path" >"$smoke_dir/host.log" 2>&1 &
    host_pid=$!
    sleep 0.1
    if kill -0 "$host_pid" 2>/dev/null; then
        break
    fi
    wait "$host_pid" 2>/dev/null || true
    host_pid=
    echo "host bind attempt $attempt failed on ephemeral port $port" >&2
done
if [[ -z "$host_pid" ]]; then
    echo "host could not bind after 3 ephemeral-port attempts" >&2
    exit 1
fi

common_env=(
    "AOE_MULTIPLAYER_PORT=$port"
    "AOE_MULTIPLAYER_AUTO_READY=1"
    "AOE_MULTIPLAYER_EXIT_TICK=12"
    "AOE_MULTIPLAYER_INPUT_DELAY=3"
    "AOE_MULTIPLAYER_SCRIPT_CHECKPOINT=12"
    "AOE_MULTIPLAYER_SCRIPT_CONTROL=1"
    "SDL_VIDEODRIVER=dummy"
    "SDL_AUDIODRIVER=dummy"
    "SDL_RENDER_DRIVER=software"
)

env "${common_env[@]}" \
    AOE_MULTIPLAYER=join \
    "AOE_MULTIPLAYER_SCRIPT_CHAT=all:Olá from join" \
    "AOE_MULTIPLAYER_STATE_PATH=$smoke_dir/join.state" \
    "AOE_SCREENSHOT_TICK=12" \
    "AOE_SCREENSHOT_PATH=$smoke_dir/join.bmp" \
    "$app_path" >"$smoke_dir/join.log" 2>&1 &
join_pid=$!

for _ in {1..80}; do
    if [[ -s "$smoke_dir/host.state" &&
          -s "$smoke_dir/join.state" ]] &&
       ! kill -0 "$host_pid" 2>/dev/null &&
       ! kill -0 "$join_pid" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

if [[ ! -s "$smoke_dir/host.state" ||
      ! -s "$smoke_dir/join.state" ]]; then
    echo "multiplayer smoke attempt timed out after 8s on port $port" >&2
    echo "host_alive=$([[ -n \"$host_pid\" ]] && kill -0 \"$host_pid\" 2>/dev/null && echo yes || echo no)" >&2
    echo "join_alive=$([[ -n \"$join_pid\" ]] && kill -0 \"$join_pid\" 2>/dev/null && echo yes || echo no)" >&2
    exit 1
fi

test -s "$smoke_dir/host.bmp"
test -s "$smoke_dir/join.bmp"
test -s "$smoke_dir/host.state"
test -s "$smoke_dir/join.state"

wait "$host_pid"
wait "$join_pid"
host_pid=
join_pid=

host_tick=$(awk '$1 == "tick" { print $2 }' "$smoke_dir/host.state")
join_tick=$(awk '$1 == "tick" { print $2 }' "$smoke_dir/join.state")
host_hash=$(awk '$1 == "hash" { print $2 }' "$smoke_dir/host.state")
join_hash=$(awk '$1 == "hash" { print $2 }' "$smoke_dir/join.state")
host_status=$(awk '$1 == "status" { print $2 }' "$smoke_dir/host.state")
join_status=$(awk '$1 == "status" { print $2 }' "$smoke_dir/join.state")

test "$host_tick" = "$join_tick"
test "$host_tick" -ge 12
test "$host_hash" = "$join_hash"
test "$host_status" = "$join_status"
test "$host_status" = "2"
test "$(awk '$1 == "input_delay" { print $2 }' \
    "$smoke_dir/host.state")" = "3"
test "$(awk '$1 == "input_delay" { print $2 }' \
    "$smoke_dir/join.state")" = "3"
grep -q '^latency_band GREEN$' "$smoke_dir/host.state"
grep -q '^checkpoint MATCHED 12$' "$smoke_dir/host.state"
grep -q '^checkpoint MATCHED 12$' "$smoke_dir/join.state"
grep -q '^paused 0$' "$smoke_dir/host.state"
grep -q '^paused 0$' "$smoke_dir/join.state"
grep -q '^speed FAST$' "$smoke_dir/host.state"
grep -q '^speed FAST$' "$smoke_dir/join.state"
grep -q '^cadence 100$' "$smoke_dir/host.state"
grep -q '^cadence 100$' "$smoke_dir/join.state"
grep -q '^pause_frozen 1$' "$smoke_dir/host.state"
test -s "$smoke_dir/checkpoint.save"
test -s "$smoke_dir/checkpoint.save.envelope"

grep -q 'chat .* blue all "Host hello"' "$smoke_dir/host.state"
grep -q 'chat .* red all "Olá from join"' "$smoke_dir/host.state"
grep -q 'chat .* blue allies "Blue allies only"' "$smoke_dir/host.state"
grep -q 'chat .* blue all "Host hello"' "$smoke_dir/join.state"
grep -q 'chat .* red all "Olá from join"' "$smoke_dir/join.state"
! grep -q 'Blue allies only' "$smoke_dir/join.state"

host_sequences=$(awk '$1 == "chat" { print $2 }' "$smoke_dir/host.state")
test "$host_sequences" = "$(printf '%s\n' "$host_sequences" | sort -n)"
