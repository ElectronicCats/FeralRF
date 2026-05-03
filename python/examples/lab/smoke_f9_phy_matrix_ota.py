#!/usr/bin/env python3
"""F9 OTA validation — 6-step PHY switch matrix with marker counts on 2 boards.

Per spec §F9 closure: 3 consecutive 6-step cycles (BLE→IEEE→Sub1G→IEEE→Sub1G→BLE)
with ≥10 OTA markers received per step. RX board cycles through PHYs without
device reset; TX board emits DEADBEEF markers in lockstep.

Hardware (per memory project_hardware.md):
- TX board #2 /dev/ttyACM5 (full hardware including Sub-1GHz TX)
- RX board #1 /dev/ttyACM8 (BLE 2.4 GHz OK; Sub-1GHz TX broken — RX still fine)

Usage:
    python smoke_f9_phy_matrix_ota.py
    python smoke_f9_phy_matrix_ota.py --tx-port /dev/ttyACM5 --rx-port /dev/ttyACM8 --cycles 3
"""

import argparse
import re
import sys
import time
import warnings

import serial

from feralrf import PHY, Radio, RxStreamError

warnings.simplefilter("ignore")

MARKER = b"\xDE\xAD\xBE\xEF"


def reset_cc1352(port):
    """Reset CC1352 via RP2040 shell port (bridge_port + 2)."""
    m = re.search(r"(\d+)$", port)
    if not m:
        return
    shell = port[: m.start(1)] + str(int(m.group(1)) + 2)
    try:
        s = serial.Serial(shell, 115200, timeout=1.0, write_timeout=1.0)
        s.write(b"boot\r\n")
        time.sleep(0.5)
        s.write(b"exit\r\n")
        time.sleep(0.3)
        s.close()
    except Exception:
        pass
    time.sleep(3.5)


def step_run(tx, rx, phy, channel, label, count, listen_extra, tx_power):
    """Configure both radios on phy/channel, TX emits markers, RX counts.

    For BLE, also sets a unique TX MAC so RX can filter against ambient lab
    traffic (lab is ~300 BLE pkts/s on ch37 — markers compete unless filtered).
    """
    tx.set_phy(phy, channel=channel)
    rx.set_phy(phy, channel=channel)

    if phy in (PHY.BLE_1M, PHY.BLE_2M, PHY.BLE_CODED_S2, PHY.BLE_CODED_S8):
        unique_mac = b"\xFE\xCA\xEF\xBE\xAD\xDE"
        tx.set_ble_addr(unique_mac)
        mac_filter = unique_mac
    else:
        mac_filter = None

    rx.start_rx()
    time.sleep(0.3)

    for _ in range(count):
        try:
            tx.transmit(MARKER, power_dbm=tx_power)
        except Exception:
            pass
        time.sleep(0.05)

    time.sleep(listen_extra)

    matched = 0
    total = 0
    for pkt in rx.read_packets(timeout=1.5):
        # F8f #7b: read_packets now also yields RxStreamError; skip those.
        if isinstance(pkt, RxStreamError):
            continue
        total += 1
        if MARKER in pkt.data:
            if mac_filter is None or mac_filter in pkt.data:
                matched += 1

    rx.stop_rx()
    return matched, total


def run_cycle(tx, rx, cycle_num, count, listen_extra, tx_power):
    print(f"\n--- cycle {cycle_num} ---")
    steps = [
        ("BLE 1M ch37", PHY.BLE_1M, 37),
        ("IEEE ch11", PHY.IEEE_802_15_4, 11),
        ("Sub1G 868", PHY.SUB_1GHZ_868, 0),
        ("IEEE ch11", PHY.IEEE_802_15_4, 11),
        ("Sub1G 868", PHY.SUB_1GHZ_868, 0),
        ("BLE 1M ch37", PHY.BLE_1M, 37),
    ]
    results = []
    for i, (label, phy, channel) in enumerate(steps, 1):
        try:
            matched, total = step_run(tx, rx, phy, channel, label, count, listen_extra, tx_power)
            status = "OK" if matched >= 10 else ("LOW" if matched > 0 else "FAIL")
            print(
                f"  step {i} {label:<14} markers={matched:>2}/{count} total_rx={total:<3} [{status}]"
            )
            results.append((label, matched, status))
        except Exception as e:
            print(f"  step {i} {label:<14} EXCEPTION: {type(e).__name__}: {e}")
            results.append((label, -1, "EXCEPTION"))
            return results, False
    cycle_pass = all(matched >= 10 for _, matched, _ in results)
    return results, cycle_pass


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--tx-port", default="/dev/ttyACM5")
    p.add_argument("--rx-port", default="/dev/ttyACM8")
    p.add_argument("--cycles", type=int, default=3)
    p.add_argument("--count", type=int, default=20, help="Markers per step")
    p.add_argument("--listen-extra", type=float, default=0.5)
    p.add_argument("--tx-power", type=int, default=5, help="TX power dBm (std PA caps at +5)")
    args = p.parse_args()

    print(f"[STEP] reset both boards (TX={args.tx_port}, RX={args.rx_port})")
    reset_cc1352(args.tx_port)
    reset_cc1352(args.rx_port)

    tx = Radio(args.tx_port)
    rx = Radio(args.rx_port)
    try:
        tx.connect()
        time.sleep(0.3)
        tx.init()
        rx.connect()
        time.sleep(0.3)
        rx.init()

        all_pass = True
        per_cycle = []
        for c in range(1, args.cycles + 1):
            results, cycle_pass = run_cycle(tx, rx, c, args.count, args.listen_extra, args.tx_power)
            per_cycle.append((c, cycle_pass, results))
            all_pass = all_pass and cycle_pass

        print("\n=== summary ===")
        for c, cycle_pass, results in per_cycle:
            steps_ok = sum(1 for _, m, _ in results if m >= 10)
            print(
                f"cycle {c}: {steps_ok}/6 steps PASS  ({'CYCLE OK' if cycle_pass else 'CYCLE FAIL'})"
            )

        print()
        if all_pass:
            print(
                f"[ OK ] F9 manual checkpoint PASS — {args.cycles}/{args.cycles} cycles, all 6 steps ≥10 markers each"
            )
            return 0
        else:
            print("[FAIL] F9 manual checkpoint FAIL — at least one step under 10 markers")
            return 1
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
