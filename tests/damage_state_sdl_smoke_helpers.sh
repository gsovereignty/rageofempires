damage_audit_jobs=${AOE_DAMAGE_AUDIT_JOBS:-4}
damage_audit_timeout=${AOE_DAMAGE_AUDIT_SAMPLE_TIMEOUT_SECONDS:-120}
case $damage_audit_jobs in
    ''|*[!0-9]*|0)
        echo "AOE_DAMAGE_AUDIT_JOBS must be a positive integer" >&2
        exit 2
        ;;
esac
case $damage_audit_timeout in
    ''|*[!0-9]*|0)
        echo "AOE_DAMAGE_AUDIT_SAMPLE_TIMEOUT_SECONDS must be a positive integer" >&2
        exit 2
        ;;
esac

damage_audit_pids=()
damage_audit_labels=()
damage_audit_watchdogs=()

cleanup_damage_audit_children() {
    if (( ${#damage_audit_pids[@]} == 0 )); then
        return
    fi
    kill "${damage_audit_pids[@]}" 2>/dev/null || true
    kill "${damage_audit_watchdogs[@]}" 2>/dev/null || true
    wait "${damage_audit_pids[@]}" 2>/dev/null || true
    wait "${damage_audit_watchdogs[@]}" 2>/dev/null || true
}

wait_for_damage_audit_batch() {
    local failed=0
    local index
    for ((index = 0; index < ${#damage_audit_pids[@]}; index++)); do
        if wait "${damage_audit_pids[$index]}"; then
            kill "${damage_audit_watchdogs[$index]}" 2>/dev/null || true
            wait "${damage_audit_watchdogs[$index]}" 2>/dev/null || true
            echo "damage-state sample complete: ${damage_audit_labels[$index]}" >&2
        else
            kill "${damage_audit_watchdogs[$index]}" 2>/dev/null || true
            wait "${damage_audit_watchdogs[$index]}" 2>/dev/null || true
            echo "damage-state sample failed: ${damage_audit_labels[$index]}" >&2
            failed=1
        fi
    done
    damage_audit_pids=()
    damage_audit_labels=()
    damage_audit_watchdogs=()
    return "$failed"
}

queue_damage_audit_sample() {
    local label=$1
    local sample_pid
    shift
    "$@" &
    sample_pid=$!
    damage_audit_pids+=("$sample_pid")
    damage_audit_labels+=("$label")
    (
        sleep "$damage_audit_timeout"
        echo "damage-state sample timed out: $label" >&2
        kill "$sample_pid" 2>/dev/null || true
    ) &
    damage_audit_watchdogs+=("$!")
    if (( ${#damage_audit_pids[@]} >= damage_audit_jobs )); then
        wait_for_damage_audit_batch
    fi
}
