#!/usr/bin/env python3
"""F26 wire-level smoke — Proprietary 2.4 GHz PHY on 2 boards.

Verifies:
  1. GFSK 250 kbps default — TX/RX 20 markers @ 2440 MHz, ≥10 received.
  2. GFSK 1 Mbps custom    — configure_prop with sym=1Mbps; ≥10 markers.
  3. CW @ 2402 MHz on PROP_2_4GHZ — ambient BLE ch37 drops to 0.
  4. No-regression BLE post-prop24g — set_phy(BLE_1M ch37) still RXes.
  5. No-regression Sub-1G hot-switch — BLE ↔ PROP_2_4GHZ ↔ Sub-1G cycle.

Hardware: TX=/dev/ttyACM5, RX=/dev/ttyACM8.
"""

import argparse
import re
import sys
import time
import warnings

import serial

from feralrf import PHY, Radio

warnings.simplefilter("ignore")

MARKER = b"\xDE\xAD\xBE\xEF"


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


def gfsk_default_check(tx, rx):
    tx.set_phy(PHY.PROP_2_4GHZ)
    rx.set_phy(PHY.PROP_2_4GHZ)
    rx.start_rx()
    time.sleep(0.3)
    for _ in range(20):
        try:
            tx.transmit(MARKER, power_dbm=5)
        except Exception:
            pass
        time.sleep(0.05)
    time.sleep(1.0)
    matched = sum(1 for pkt in rx.read_packets(timeout=1.5) if MARKER in pkt.data)
    rx.stop_rx()
    ok = matched >= 10
    print(f"  GFSK 250k default markers={matched}/20 {'PASS' if ok else 'FAIL'}")
    return ok


def gfsk_1mbps_check(tx, rx):
    """Use configure_prop to drop both boards into 1 Mbps GFSK."""
    cfg = dict(
        frequency_hz=2440000000,
        mod_type=1,  # GFSK
        symbol_rate=1000000,
        deviation=500000,
        rx_bw=0x59,
        sync_word=0x930B51DE,
        format_conf=0,
    )
    tx.set_phy(PHY.PROP_2_4GHZ)
    tx.configure_prop(**cfg)
    rx.set_phy(PHY.PROP_2_4GHZ)
    rx.configure_prop(**cfg)

    rx.start_rx()
    time.sleep(0.3)
    for _ in range(20):
        try:
            tx.transmit(MARKER, power_dbm=5)
        except Exception:
            pass
        time.sleep(0.05)
    time.sleep(1.0)
    matched = sum(1 for pkt in rx.read_packets(timeout=1.5) if MARKER in pkt.data)
    rx.stop_rx()
    ok = matched >= 10
    print(f"  GFSK 1Mbps custom markers={matched}/20 {'PASS' if ok else 'FAIL'}")
    return ok


def cw_2402_jam_check(tx, rx):
    """CW @ 2402 MHz via PROP_2_4GHZ jams BLE ch37 ambient."""
    rx.set_phy(PHY.BLE_1M, channel=37)
    rx.start_rx()
    time.sleep(2.0)
    n_idle = sum(1 for _ in rx.read_packets(timeout=0.5))
    rx.stop_rx()

    tx.set_phy(PHY.PROP_2_4GHZ, frequency_hz=2402000000)
    tx.tx_cw(power_dbm=5)
    rx.start_rx()
    time.sleep(2.0)
    n_cw = sum(1 for _ in rx.read_packets(timeout=0.5))
    rx.stop_rx()
    tx.tx_test_stop()

    drop = n_idle - n_cw
    ok = n_idle > 30 and drop > n_idle * 0.8
    print(f"  CW@2402 idle={n_idle} cw_on={n_cw} drop={drop:+d} {'PASS' if ok else 'FAIL'}")
    return ok


def ble_post_prop24g_check(rx):
    rx.set_phy(PHY.BLE_1M, channel=37)
    rx.start_rx()
    time.sleep(2.0)
    n = sum(1 for _ in rx.read_packets(timeout=0.5))
    rx.stop_rx()
    ok = n > 30
    print(f"  BLE post-prop24g pkts={n} {'PASS' if ok else 'FAIL'}")
    return ok


def hot_switch_cycle_check(rx):
    """BLE → PROP_2_4GHZ → SUB_1GHZ_868 → BLE without errors."""
    try:
        rx.set_phy(PHY.BLE_1M, channel=37)
        rx.set_phy(PHY.PROP_2_4GHZ)
        rx.set_phy(PHY.SUB_1GHZ_868)
        rx.set_phy(PHY.BLE_1M, channel=37)
        print("  hot-switch BLE→prop24g→Sub1G→BLE: PASS")
        return True
    except Exception as e:
        print(f"  hot-switch BLE→prop24g→Sub1G→BLE: FAIL ({type(e).__name__}: {e})")
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
        results["gfsk_250k"] = gfsk_default_check(tx, rx)
        results["gfsk_1mbps"] = gfsk_1mbps_check(tx, rx)
        results["cw_2402"] = cw_2402_jam_check(tx, rx)
        results["ble_post"] = ble_post_prop24g_check(rx)
        results["hot_switch"] = hot_switch_cycle_check(rx)

        # Pass criteria from spec §5.1: tests 1, 2, 4 must pass.
        core_pass = results["gfsk_250k"] and results["gfsk_1mbps"] and results["ble_post"]
        n_pass = sum(results.values())
        print()
        print(
            f"[ {'OK' if core_pass else 'FAIL'} ] F26 smoke: {n_pass}/5 PASS "
            f"(core 1+2+4: {'PASS' if core_pass else 'FAIL'})"
        )
        return 0 if core_pass else 1
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
