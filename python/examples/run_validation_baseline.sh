#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

export PYTHONPATH="${REPO_ROOT}/python${PYTHONPATH:+:${PYTHONPATH}}"

PORT=""
BAUDRATE="921600"
WITH_OOK="0"
FAILURES=0

usage() {
    cat <<'EOF'
Usage: python/examples/run_validation_baseline.sh [--port PORT] [--baudrate BAUD] [--with-ook]

Runs the current baseline validation sweep using existing smoke scripts plus
the baseline helpers added for BLE scan mode and proprietary presets.

Notes:
- This wrapper validates command path and basic RX/TX flows.
- Quiet RF environments may require per-script --min-packets tuning.
- OOK is skipped by default because it requires reset/recovery handling.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --port|-p)
            PORT="${2:-}"
            shift 2
            ;;
        --baudrate|-b)
            BAUDRATE="${2:-}"
            shift 2
            ;;
        --with-ook)
            WITH_OOK="1"
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "[FAIL] Unknown argument: $1"
            usage
            exit 2
            ;;
    esac
done

common_args=("--baudrate" "${BAUDRATE}")
if [[ -n "${PORT}" ]]; then
    common_args+=("--port" "${PORT}")
fi

run_step() {
    local label="$1"
    shift
    echo
    echo "[STEP] ${label}"
    echo "[CMD ] $*"
    if "$@"; then
        echo "[ OK ] ${label}"
    else
        local rc=$?
        echo "[FAIL] ${label} (exit=${rc})"
        FAILURES=$((FAILURES + 1))
    fi
}

echo "FeralRF Validation Baseline"
echo "==========================="
echo "repo=${REPO_ROOT}"
echo "port=${PORT:-auto} baudrate=${BAUDRATE} with_ook=${WITH_OOK}"

run_step "BLE 1M control path" \
    python3 "${SCRIPT_DIR}/smoke_phase2.py" "${common_args[@]}" --phy 0 --channel 37 --power 0

run_step "BLE passive scan" \
    python3 "${SCRIPT_DIR}/smoke_ble_scan_mode.py" "${common_args[@]}" --mode passive --channel 37 --duration 5

run_step "BLE active scan" \
    python3 "${SCRIPT_DIR}/smoke_ble_scan_mode.py" "${common_args[@]}" --mode active --channel 37 --duration 5

run_step "BLE 1M TX raw" \
    python3 "${SCRIPT_DIR}/smoke_tx_ble_phase1.py" "${common_args[@]}" --channel 37 --power 0

run_step "BLE 1M TX frame" \
    python3 "${SCRIPT_DIR}/smoke_tx_frame_phase1.py" "${common_args[@]}" --phy 0 --channel 37 --power 0 --frame-hex 020106

run_step "BLE 2M control path" \
    python3 "${SCRIPT_DIR}/smoke_phase2.py" "${common_args[@]}" --phy 1 --channel 37 --power 0

run_step "BLE Coded S8 control path" \
    python3 "${SCRIPT_DIR}/smoke_phase2.py" "${common_args[@]}" --phy 2 --channel 37 --power 0

run_step "BLE Coded S2 control path" \
    python3 "${SCRIPT_DIR}/smoke_phase2.py" "${common_args[@]}" --phy 3 --channel 37 --power 0

run_step "IEEE 802.15.4 RX" \
    python3 "${SCRIPT_DIR}/smoke_phy4_ieee154.py" "${common_args[@]}" --channel 25 --duration 5

run_step "IEEE 802.15.4 TX raw" \
    python3 "${SCRIPT_DIR}/smoke_tx_phase1.py" "${common_args[@]}" --phy 4 --channel 25 --power 0

run_step "IEEE 802.15.4 TX frame" \
    python3 "${SCRIPT_DIR}/smoke_tx_frame_phase1.py" "${common_args[@]}" --phy 4 --channel 25 --power 0

run_step "IEEE 802.15.4 TX burst" \
    python3 "${SCRIPT_DIR}/smoke_tx_burst_phase1.py" "${common_args[@]}" --phy 4 --channel 25 --power 0

run_step "IEEE 802.15.4 TX continuous" \
    python3 "${SCRIPT_DIR}/smoke_tx_continuous_phase1.py" "${common_args[@]}" --phy 4 --channel 25 --power 0 --run-seconds 1

run_step "Proprietary GFSK 433 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset gfsk_433_50k --power 0

run_step "Proprietary FSK 433 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset fsk_433_50k --power 0

run_step "Sub-1GHz 868 GFSK preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset gfsk_868_50k --power 0

run_step "Sub-1GHz 915 GFSK preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset gfsk_915_50k --power 0

run_step "Proprietary 2.4 GHz GFSK preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset gfsk_2440_50k --power 0

if [[ "${WITH_OOK}" == "1" ]]; then
    run_step "OOK 433 preset with recovery" \
        python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset ook_433_4k8 --power 0 --auto-reset

    run_step "OOK 868 preset with recovery" \
        python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset ook_868_4k8 --power 0 --auto-reset
else
    echo
    echo "[INFO] OOK presets skipped. Re-run with --with-ook to validate OOK + reset recovery."
fi

echo
if [[ "${FAILURES}" -eq 0 ]]; then
    echo "[ OK ] Validation baseline PASS"
    exit 0
fi

echo "[FAIL] Validation baseline finished with ${FAILURES} failing step(s)"
exit 1
