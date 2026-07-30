#!/usr/bin/env bash
set -euo pipefail

app=${1:?SDL roster multiplayer executable required}
root=$(mktemp -d)
pids=()
cleanup() {
    for pid in ${pids[*]-}; do kill "$pid" 2>/dev/null || true; done
    rm -rf "$root"
}
trap cleanup EXIT

roster=0,1,2,3,4,5,6,7
checkpoint_save="$root/checkpoint.save"
checkpoint_envelope="$root/checkpoint.envelope"

wait_file() {
    local file=$1
    local limit=$((SECONDS + 12))
    while [ ! -s "$file" ]; do
        if (( SECONDS >= limit )); then return 1; fi
        sleep 0.02
    done
}

run_phase() {
    local phase=$1
    local port=$((43000 + ($$ % 8000) + phase))
    pids=()
    for slot in 0 1 2 3 4 5 6 7; do
        mode=join
        [ "$slot" -eq 0 ] && mode=host
        resume=0
        [ "$phase" -eq 2 ] && resume=1
        env SDL_VIDEODRIVER=dummy \
            AOE_MULTIPLAYER="$mode" \
            AOE_MULTIPLAYER_PORT="$port" \
            AOE_MULTIPLAYER_ROSTER="$roster" \
            AOE_MULTIPLAYER_LOCAL_SLOT="$slot" \
            AOE_MULTIPLAYER_READY_PATH="$root/ready-$phase" \
            AOE_MULTIPLAYER_CONNECTED_PATH="$root/connected-$phase-$slot" \
            AOE_MULTIPLAYER_CHECKPOINT_SAVE="$checkpoint_save" \
            AOE_MULTIPLAYER_CHECKPOINT_ENVELOPE="$checkpoint_envelope" \
            AOE_MULTIPLAYER_RESUME="$resume" \
            AOE_MULTIPLAYER_SCREENSHOT_PATH="$root/phase-$phase-slot-$slot.bmp" \
            AOE_MULTIPLAYER_STATE_PATH="$root/phase-$phase-slot-$slot.state" \
            "$app" >"$root/phase-$phase-slot-$slot.log" 2>&1 &
        pids+=("$!")
        if [ "$slot" -eq 0 ]; then
            wait_file "$root/ready-$phase"
        else
            wait_file "$root/connected-$phase-$slot"
        fi
    done
    local deadline=$((SECONDS + 35))
    for pid in "${pids[@]}"; do
        while kill -0 "$pid" 2>/dev/null; do
            if (( SECONDS >= deadline )); then return 1; fi
            sleep 0.02
        done
        wait "$pid"
    done
    pids=()
}

run_phase 1
test -s "$checkpoint_save"
test -s "$checkpoint_envelope"
run_phase 2

for phase in 1 2; do
    expected_reconnect=ready
    [ "$phase" -eq 2 ] && expected_reconnect=verified
    first_hash=
    for slot in 0 1 2 3 4 5 6 7; do
        state="$root/phase-$phase-slot-$slot.state"
        image="$root/phase-$phase-slot-$slot.bmp"
        grep -qx "protocol 3" "$state"
        grep -qx "roster $roster" "$state"
        grep -qx "participants 8" "$state"
        grep -qx "checkpoint matched" "$state"
        grep -qx "reconnect $expected_reconnect" "$state"
        test -s "$image"
        hash=$(awk '/^hash / {print $2}' "$state")
        test -n "$hash"
        if [ -z "$first_hash" ]; then first_hash=$hash; else
            test "$first_hash" = "$hash"
        fi
    done
done

echo "eight-slot SDL checkpoint/reconnect smoke passed"
