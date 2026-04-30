#!/usr/bin/env python3
"""F22 wire-level smoke — CW + PRBS test modes on 2 boards.

Verifies:
  1. CW on Sub-1GHz 868 — firmware ACKs command (no exception).
  2. CW on BLE 1M ch37 — proves the chip really is on-air via OTA
     interference: lab ambient ~150-200 pkts/s normally; with CW running
     ch37 should drop near 0 (single tone jams BLE ADV decoding).
  3. PRBS-15 on Sub-1GHz 868 — firmware ACKs command (PRBS path identical
     to CW path; OTA spectral validation requires analyzer per spec §F22).
  4. PRBS-32 on Sub-1GHz 868 — firmware ACKs command.
  5. tx_test_stop is idempotent — safe to call when nothing is running.

Hardware: TX=/dev/ttyACM5 (board #2), RX=/dev/ttyACM8 (board #1).
"""

import argparse
import re
import sys
import time
import warnings

import serial

from feralrf import PHY, Radio

warnings.simplefilter("ignore")


def reset(port):
    m = re.search(r"(\d+)$", port)
    if not m:
        return
    shell = port[: m.start(1)] + str(int(m.group(1)) + 2)
    try:
        s = serial.Serial(shell, 115200, timeout=1.0)
        s.write(b"boot\r\n")
        time.sleep(0.5)
        s.write(b"exit\r\n")
        time.sleep(0.3)
        s.close()
    except Exception:
        pass
    time.sleep(3.5)


def cw_check(tx, label, phy, channel=0):
    """tx_cw passes if firmware ACKs the CMD_TX_CW (no exception raised).

    The firmware path: command_processor → RadioIF_runTxTest opens the RF
    handle if needed, posts CMD_TX_TEST. ACK only sent on RF_postCmd
    success. So the absence of an exception is the validation signal.
    """
    try:
        tx.set_phy(phy, channel=channel)
        tx.tx_cw(power_dbm=5)
        time.sleep(0.3)
        tx.tx_test_stop()
        print(f"  CW {label:<14} accepted by firmware: PASS")
        return True
    except Exception as e:
        print(f"  CW {label:<14} FAILED: {type(e).__name__}: {e}")
        return False


def cw_interference_check(tx, rx):
    """CW jam test on BLE 1M ch37 — proves carrier really hits the air.

    Lab ch37 has ~150-200 BLE ADV pkts/s. CW (single tone) at 2402 MHz
    drowns out the modem's sync detector → RX pkt count drops to ~0.
    Strong OTA evidence the carrier is actually transmitting.
    """
    rx.set_phy(PHY.BLE_1M, channel=37)
    rx.start_rx()
    time.sleep(2.0)
    n_idle = sum(1 for _ in rx.read_packets(timeout=0.5))
    rx.stop_rx()

    tx.set_phy(PHY.BLE_1M, channel=37)
    tx.tx_cw(power_dbm=5)
    rx.start_rx()
    time.sleep(2.0)
    n_cw_active = sum(1 for _ in rx.read_packets(timeout=0.5))
    rx.stop_rx()
    tx.tx_test_stop()

    drop = n_idle - n_cw_active
    ok = n_idle > 30 and drop > n_idle * 0.8
    print(
        f"  CW interference  idle={n_idle:>3} cw_on={n_cw_active:>3} "
        f"drop={drop:+d} {'PASS' if ok else 'FAIL'}"
    )
    return ok


def prbs_ack_check(tx, label, pattern):
    """PRBS validation via firmware ACK only.

    Same firmware path as CW (which we proved emits on-air via the
    interference test). PRBS spreads energy across modulation bandwidth,
    so it doesn't jam ambient BLE as cleanly — full OTA spectral check
    requires a spectrum analyzer (manual checkpoint per spec §F22).
    """
    try:
        tx.set_phy(PHY.SUB_1GHZ_868, channel=0)
        tx.tx_prbs(power_dbm=5, pattern=pattern)
        time.sleep(0.3)
        tx.tx_test_stop()
        print(f"  PRBS {label:<6} accepted by firmware: PASS")
        return True
    except Exception as e:
        print(f"  PRBS {label:<6} FAILED: {type(e).__name__}: {e}")
        return False


def stop_idempotent(tx):
    try:
        tx.tx_test_stop()
        tx.tx_test_stop()  # second call should not raise
        print("  tx_test_stop idempotent: PASS")
        return True
    except Exception as e:
        print(f"  tx_test_stop idempotent: FAIL ({e})")
        return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tx-port", default="/dev/ttyACM5")
    ap.add_argument("--rx-port", default="/dev/ttyACM8")
    args = ap.parse_args()

    print("[STEP] reset both")
    reset(args.tx_port)
    reset(args.rx_port)

    tx = Radio(args.tx_port)
    rx = Radio(args.rx_port)
    results = {}
    try:
        tx.connect()
        time.sleep(0.3)
        tx.init()
        rx.connect()
        time.sleep(0.3)
        rx.init()

        print("[STEP] tests")
        # CW interference goes first while RF state is fresh — strongest evidence.
        results["cw_interference"] = cw_interference_check(tx, rx)
        results["cw_sub1g"] = cw_check(tx, "Sub1G_868", PHY.SUB_1GHZ_868)
        results["prbs15"] = prbs_ack_check(tx, "PRBS15", "prbs15")
        results["prbs32"] = prbs_ack_check(tx, "PRBS32", "prbs32")
        results["idempotent"] = stop_idempotent(tx)

        all_ok = all(results.values())
        print()
        print(f"[ {'OK' if all_ok else 'FAIL'} ] F22 smoke: " f"{sum(results.values())}/5 PASS")
        return 0 if all_ok else 1
    finally:
        try:
            tx.disconnect()
        except Exception:
            pass
        try:
            rx.disconnect()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
