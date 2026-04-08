#!/usr/bin/env bash

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

export PYTHONPATH="${REPO_ROOT}/python${PYTHONPATH:+:${PYTHONPATH}}"

PORT=""
RX_PORT=""
BAUDRATE="921600"
ONLY=""
FAILURES=0

usage() {
    cat <<'EOF'
Usage: python/examples/run_validation_baseline.sh --port PORT [--rx-port PORT2] [--baudrate BAUD] [--only FILTER]

Runs the full baseline validation sweep with CC1352 reset between each step.

Options:
  --port PORT       TX board (or single board for control-path tests)
  --rx-port PORT2   RX board for OTA marker validation (TX->RX between 2 boards).
                    Adds OTA steps for PHYs and presets that support raw TX/RX.
                    Does not cover TX_FRAME, TX_BURST, TX_CONTINUOUS, or BLE scans.
  --only FILTER     Run only steps whose label contains FILTER (case-insensitive).
                    Examples: --only "OOK 868", --only 433, --only BLE, --only IEEE

Notes:
- Resets CC1352 via RP2040 shell between every test for clean RF state.
- 433 MHz tests run last (marginal link on CatSniffer antenna).
- OOK runs last (locks radio — power cycle required after).
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
        --rx-port)
            RX_PORT="${2:-}"
            shift 2
            ;;
        --only|-o)
            ONLY="${2:-}"
            shift 2
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

_reset_one() {
    local p="$1"
    python3 -c "
import serial, time, re, sys
port = '${p}'
m = re.search(r'(\d+)$', port)
if not m: sys.exit(1)
shell_port = port[:m.start(1)] + str(int(m.group(1)) + 2)
try:
    s = serial.Serial(shell_port, 115200, timeout=1.0, write_timeout=1.0)
    s.write(b'boot\r\n')
    time.sleep(0.5)
    s.write(b'exit\r\n')
    time.sleep(0.3)
    s.close()
    time.sleep(3.5)
except Exception:
    sys.exit(1)
" 2>/dev/null
}

reset_device() {
    # Reset CC1352 via RP2040 shell between tests to ensure clean RF state.
    local tx_ok=0 rx_ok=0
    if [[ -n "${PORT}" ]]; then
        if _reset_one "${PORT}"; then
            tx_ok=1
        fi
        if [[ -n "${RX_PORT}" ]]; then
            if _reset_one "${RX_PORT}"; then
                rx_ok=1
            fi
            if [[ ${tx_ok} -eq 1 && ${rx_ok} -eq 1 ]]; then
                echo "[RST ] CC1352 reset OK (TX+RX)"
            elif [[ ${tx_ok} -eq 1 ]]; then
                echo "[RST ] CC1352 TX reset OK, RX reset FAILED"
            elif [[ ${rx_ok} -eq 1 ]]; then
                echo "[RST ] CC1352 TX reset FAILED, RX reset OK"
            else
                echo "[RST ] CC1352 reset FAILED (TX+RX)"
            fi
        else
            if [[ ${tx_ok} -eq 1 ]]; then
                echo "[RST ] CC1352 reset OK"
            else
                echo "[RST ] CC1352 reset FAILED"
            fi
        fi
    fi
}

run_step() {
    local label="$1"
    shift
    # Skip if --only filter is set and label doesn't match
    if [[ -n "${ONLY}" ]]; then
        local label_lower="${label,,}"
        local only_lower="${ONLY,,}"
        if [[ "${label_lower}" != *"${only_lower}"* ]]; then
            return 0
        fi
    fi
    echo
    reset_device
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

run_ota() {
    # Run OTA marker test if --rx-port is set
    local label="$1"
    shift
    if [[ -z "${RX_PORT}" ]]; then
        return 0
    fi
    # Skip if --only filter doesn't match
    if [[ -n "${ONLY}" ]]; then
        local label_lower="${label,,}"
        local only_lower="${ONLY,,}"
        if [[ "${label_lower}" != *"${only_lower}"* ]]; then
            return 0
        fi
    fi
    reset_device
    echo "[OTA ] ${label}"
    if python3 "${SCRIPT_DIR}/smoke_ota_txrx.py" --tx-port "${PORT}" --rx-port "${RX_PORT}" --baudrate "${BAUDRATE}" "$@"; then
        echo "[ OK ] ${label} OTA"
    else
        local rc=$?
        echo "[FAIL] ${label} OTA (exit=${rc})"
        FAILURES=$((FAILURES + 1))
    fi
}

echo "FeralRF Validation Baseline"
echo "==========================="
echo "repo=${REPO_ROOT}"
echo "port=${PORT:-auto} rx_port=${RX_PORT:-none} baudrate=${BAUDRATE} only=${ONLY:-all}"

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

run_ota "BLE 1M OTA" --phy 0 --channel 37
run_ota "BLE 2M OTA" --phy 1 --channel 37
run_ota "BLE Coded S8 OTA" --phy 2 --channel 37
run_ota "BLE Coded S2 OTA" --phy 3 --channel 37

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

run_ota "IEEE 802.15.4 OTA" --phy 4 --channel 25
run_ota "Sub-1GHz 868 OTA" --phy 5 --channel 0
run_ota "Sub-1GHz 915 OTA" --phy 6 --channel 0

run_step "Sub-1GHz 868 GFSK preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset gfsk_868_50k --power 0

run_step "Sub-1GHz 915 GFSK preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset gfsk_915_50k --power 0

run_step "Proprietary 2.4 GHz GFSK preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset gfsk_2440_50k --power 0

run_step "MSK 868 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset msk_868_50k --power 0

run_ota "GFSK 868 OTA" --preset gfsk_868_50k
run_ota "GFSK 915 OTA" --preset gfsk_915_50k
run_ota "GFSK 2440 OTA" --preset gfsk_2440_50k
run_ota "MSK 868 OTA" --preset msk_868_50k

run_step "4-FSK 868 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset 4fsk_868_50k --power 0

run_step "4-GFSK 868 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset 4gfsk_868_50k --power 0

run_ota "4-FSK 868 OTA" --preset 4fsk_868_50k
run_ota "4-GFSK 868 OTA" --preset 4gfsk_868_50k

run_step "W-MBus S-mode 868 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset wireless_mbus_s_868 --power 0

run_step "W-MBus T-mode 868 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset wireless_mbus_t_868 --power 0

run_step "W-MBus C-mode 868 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset wireless_mbus_c_868 --power 0

run_ota "W-MBus S OTA" --preset wireless_mbus_s_868
run_ota "W-MBus T OTA" --preset wireless_mbus_t_868
run_ota "W-MBus C OTA" --preset wireless_mbus_c_868

# --- 433 MHz tests last (marginal link on CatSniffer antenna) ---

run_step "Proprietary GFSK 433 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset gfsk_433_50k --power 0

run_step "Proprietary FSK 433 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset fsk_433_50k --power 0

run_step "MSK 433 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset msk_433_50k --power 0

run_ota "GFSK 433 OTA" --preset gfsk_433_50k
run_ota "FSK 433 OTA" --preset fsk_433_50k
run_ota "MSK 433 OTA" --preset msk_433_50k

run_step "4-FSK 433 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset 4fsk_433_50k --power 0

run_step "4-GFSK 433 preset" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset 4gfsk_433_50k --power 0

run_ota "4-FSK 433 OTA" --preset 4fsk_433_50k
run_ota "4-GFSK 433 OTA" --preset 4gfsk_433_50k

# --- OOK last (locks radio — requires power cycle after) ---

run_step "OOK 868 preset with recovery" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset ook_868_4k8 --power 0 --auto-reset

run_ota "OOK 868 OTA" --preset ook_868_4k8
# OOK 433 OTA skipped — CatSniffer antenna + OOK sensitivity = no link at 433

run_step "OOK 433 preset with recovery" \
    python3 "${SCRIPT_DIR}/smoke_prop_phase1.py" "${common_args[@]}" --preset ook_433_4k8 --power 0 --auto-reset

echo
if [[ "${FAILURES}" -eq 0 ]]; then
    echo "[ OK ] Validation baseline PASS"
    exit 0
fi

echo "[FAIL] Validation baseline finished with ${FAILURES} failing step(s)"
exit 1
