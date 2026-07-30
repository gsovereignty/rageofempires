#!/usr/bin/env bash
set -euo pipefail

app=${1:?headless multiplayer executable required}
root=$(mktemp -d)
pids=()
cleanup() {
    for pid in ${pids[*]-}; do kill "$pid" 2>/dev/null || true; done
    rm -rf "$root"
}
trap cleanup EXIT

port=$((42000 + ($$ % 10000)))

dump_failure() {
    echo "three-peer roster smoke failed" >&2
    for file in "$root"/*; do
        [ -f "$file" ] || continue
        echo "== $(basename "$file") ==" >&2
        case "$file" in
            *.ppm) echo "binary screenshot $(wc -c < "$file") bytes" >&2 ;;
            *) sed -n '1,240p' "$file" >&2 ;;
        esac
    done
}

wait_file() {
    local file=$1
    local limit=$((SECONDS + 8))
    while [ ! -s "$file" ]; do
        if (( SECONDS >= limit )); then dump_failure; return 1; fi
        sleep 0.02
    done
}

run_peer() {
    local mode=$1 slot=$2
    AOE_MULTIPLAYER="$mode" \
    AOE_MULTIPLAYER_PORT="$port" \
    AOE_MULTIPLAYER_ROSTER=0,1,2 \
    AOE_MULTIPLAYER_LOCAL_SLOT="$slot" \
    AOE_MULTIPLAYER_READY_PATH="$root/ready" \
    AOE_MULTIPLAYER_CONNECTED_PATH="$root/connected-$slot" \
    AOE_MULTIPLAYER_CHECKPOINT_SAVE="$root/checkpoint.save" \
    AOE_MULTIPLAYER_CHECKPOINT_ENVELOPE="$root/checkpoint.envelope" \
    AOE_MULTIPLAYER_SCREENSHOT_PATH="$root/slot-$slot.ppm" \
    AOE_MULTIPLAYER_STATE_PATH="$root/slot-$slot.state" \
        "$app" >"$root/slot-$slot.log" 2>&1 &
    pids+=("$!")
}

run_peer host 0
wait_file "$root/ready"
run_peer join 1
wait_file "$root/connected-1"
run_peer join 2
wait_file "$root/connected-2"

deadline=$((SECONDS + 20))
for pid in "${pids[@]}"; do
    while kill -0 "$pid" 2>/dev/null; do
        if (( SECONDS >= deadline )); then dump_failure; exit 1; fi
        sleep 0.02
    done
    if ! wait "$pid"; then dump_failure; exit 1; fi
done
pids=()

for slot in 0 1 2; do
    state="$root/slot-$slot.state"
    image="$root/slot-$slot.ppm"
    grep -qx "protocol 3" "$state"
    grep -qx "roster 0,1,2" "$state"
    grep -qx "slot $slot" "$state"
    grep -qx "participants 3" "$state"
    grep -qx "tick 3" "$state"
    grep -qx "signals 1" "$state"
    grep -qx "status running" "$state"
    grep -qx "checkpoint matched" "$state"
    grep -qx "reconnect ready" "$state"
    test -s "$image"
done

hash0=$(awk '/^hash / {print $2}' "$root/slot-0.state")
hash1=$(awk '/^hash / {print $2}' "$root/slot-1.state")
hash2=$(awk '/^hash / {print $2}' "$root/slot-2.state")
test -n "$hash0"
test "$hash0" = "$hash1"
test "$hash0" = "$hash2"
cmp "$root/slot-0.ppm" "$root/slot-1.ppm"
cmp "$root/slot-0.ppm" "$root/slot-2.ppm"
echo "three-peer protocol-3 headless smoke passed: $hash0"
