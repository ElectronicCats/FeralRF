#!/usr/bin/env python3
"""F17 — Device emulation smoke V1 (cross-validation 2 boards).

For each personality across all 4 protocols, TX board emulates → RX board
verifies signature presence in received packets. Pass criteria:
  - BLE: >= 1 packet with target MAC in payload
  - IEEE154: >= 8/20 packets CRC OK with FCF byte match
  - Sub-1GHz: >= 8/20 markers payload match
  - OOK: >= 5/20 markers payload match (less reliable demodulation)

Usage:
    python smoke_f17_emulation.py --tx-port /dev/ttyACM1 --rx-port /dev/ttyACM2
"""
import argparse
import re
import sys
import time

import serial

from feralrf import PHY, PROP_PRESETS, Radio, RxStreamError
from feralrf.emulation import (
    BLE_PERSONALITIES,
    IEEE154_PERSONALITIES,
    OOK_PERSONALITIES,
    SUB1GHZ_PERSONALITIES,
    emulate_ble,
    emulate_ieee154,
    emulate_ook,
    emulate_sub1ghz,
)


def reset_cc1352(port: str) -> None:
    """Reset CC1352 via RP2040 shell port (data port + 2)."""
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


def run_ble_personality(tx_port, rx_port, baud, personality, count):
    """BLE: RX raw + TX emulate_ble. Pass = >=1 packet with target MAC LE."""
    reset_cc1352(tx_port)
    reset_cc1352(rx_port)
    tx = Radio(port=tx_port, baudrate=baud)
    rx = Radio(port=rx_port, baudrate=baud)
    try:
        tx.init()
        rx.init()
        rx.set_phy(PHY.BLE_1M, channel=37)
        rx.start_rx()
        time.sleep(0.3)
        emulate_ble(tx, personality, count=count, interval_ms=100)
        time.sleep(1.0)
        target_le = bytes(reversed(bytes.fromhex(personality.target_mac.replace(":", ""))))
        matched = 0
        total = 0
        for pkt in rx.read_packets(timeout=2.0):
            if isinstance(pkt, RxStreamError):
                continue
            total += 1
            if target_le in pkt.data:
                matched += 1
        try:
            rx.stop_rx()
        except Exception:
            pass
        return matched, total
    finally:
        tx.disconnect()
        rx.disconnect()


def run_ieee154_personality(tx_port, rx_port, baud, personality, count):
    """IEEE154: RX raw on channel + TX. Pass = >=8 with FCF match."""
    reset_cc1352(tx_port)
    reset_cc1352(rx_port)
    tx = Radio(port=tx_port, baudrate=baud)
    rx = Radio(port=rx_port, baudrate=baud)
    try:
        tx.init()
        rx.init()
        rx.set_phy(PHY.IEEE_802_15_4, channel=personality.channel)
        rx.start_rx()
        time.sleep(0.3)
        emulate_ieee154(tx, personality, count=count, interval_ms=100)
        time.sleep(1.0)
        fcf = personality.payload[:2]
        matched = 0
        total = 0
        for pkt in rx.read_packets(timeout=2.0):
            if isinstance(pkt, RxStreamError):
                continue
            total += 1
            if pkt.crc_ok and fcf in pkt.data[:8]:
                matched += 1
        try:
            rx.stop_rx()
        except Exception:
            pass
        return matched, total
    finally:
        tx.disconnect()
        rx.disconnect()


def run_subg_personality(tx_port, rx_port, baud, personality, count):
    """Sub-1GHz: RX with same preset + TX. Pass = >=8 payload match.

    TX power boosted to 10 dBm — 433 MHz is marginal at 0 dBm per
    project_433mhz_root_cause memory (CatSniffer matching network optimized
    for 868/915, ~7 dB less link margin at 433).
    """
    reset_cc1352(tx_port)
    reset_cc1352(rx_port)
    tx = Radio(port=tx_port, baudrate=baud)
    rx = Radio(port=rx_port, baudrate=baud)
    try:
        tx.init()
        rx.init()
        rx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
        rx.configure_prop(**PROP_PRESETS[personality.preset_name])
        rx.start_rx()
        time.sleep(0.3)
        emulate_sub1ghz(tx, personality, count=count, interval_ms=100, power_dbm=10)
        time.sleep(1.0)
        sig = personality.payload[:4]
        matched = 0
        total = 0
        for pkt in rx.read_packets(timeout=2.0):
            if isinstance(pkt, RxStreamError):
                continue
            total += 1
            if pkt.crc_ok and sig in pkt.data:
                matched += 1
        try:
            rx.stop_rx()
        except Exception:
            pass
        return matched, total
    finally:
        tx.disconnect()
        rx.disconnect()


def run_ook_personality(tx_port, rx_port, baud, personality, count):
    """OOK: RX with same OOK preset + TX. Pass = >=5 payload match.

    Extra 2.5s settling delay after reset because OOK init after MSK 868
    transitions can race the bootloader (observed 2026-05-04 first run:
    "Radio init failed" on all 3 OOK personalities).
    """
    reset_cc1352(tx_port)
    reset_cc1352(rx_port)
    time.sleep(2.5)  # extra settling for OOK init post-non-OOK preset
    tx = Radio(port=tx_port, baudrate=baud)
    rx = Radio(port=rx_port, baudrate=baud)
    try:
        tx.init()
        rx.init()
        rx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
        rx.configure_prop(**PROP_PRESETS[personality.preset_name])
        rx.start_rx()
        time.sleep(0.3)
        # auto_reset=False — smoke loop will reset between personalities anyway
        emulate_ook(tx, personality, count=count, interval_ms=100, auto_reset=False)
        time.sleep(1.0)
        sig = personality.payload[:3]
        matched = 0
        total = 0
        for pkt in rx.read_packets(timeout=2.0):
            if isinstance(pkt, RxStreamError):
                continue
            total += 1
            if sig in pkt.data:
                matched += 1
        try:
            rx.stop_rx()
        except Exception:
            pass
        return matched, total
    finally:
        tx.disconnect()
        rx.disconnect()


def run_with_retry(fn, *args):
    """Per F29.b lesson: retry once on transient TimeoutError during reset/init."""
    last_exc = None
    for attempt in range(2):
        try:
            return fn(*args), None
        except Exception as exc:
            last_exc = exc
            if attempt == 0:
                print(f"  [WARN] {exc!r} — retry 1/2")
    return (0, 0), last_exc


def main() -> int:
    parser = argparse.ArgumentParser(description="F17 emulation smoke V1")
    parser.add_argument("--tx-port", required=True)
    parser.add_argument("--rx-port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--count", type=int, default=20)
    parser.add_argument(
        "--skip-ook",
        action="store_true",
        help="Skip OOK personalities (firmware-level lock-up bug per "
        "project_ook_bug memory; OOK validation requires reflash between "
        "personalities — F17.b).",
    )
    parser.add_argument(
        "--skip-433",
        action="store_true",
        help="Skip 433 MHz personalities. CatSniffer matching network is "
        "optimized for 868/915 — 433 has ~7 dB less link margin and on "
        "this specific hardware demodulation is too marginal for marker tests.",
    )
    args = parser.parse_args()

    thresholds = {
        "BLE": 1,
        "IEEE154": 8,
        "SUB1G": 8,
        "OOK": 5,
    }

    print("F17 Device Emulation Smoke V1")
    print(f"TX={args.tx_port} RX={args.rx_port} count={args.count} skip_ook={args.skip_ook}")
    print("=" * 60)

    def _filter_433(personalities):
        if not args.skip_433:
            return personalities
        # Filter out personalities whose name or preset has "433" — for this
        # specific CatSniffer hardware where 433 is marginal.
        out = []
        for p in personalities:
            preset = getattr(p, "preset_name", "")
            if "_433" in p.name or "_433" in preset or "433" in p.name:
                continue
            out.append(p)
        return tuple(out)

    plan = [
        ("BLE", _filter_433(BLE_PERSONALITIES), run_ble_personality),
        ("IEEE154", _filter_433(IEEE154_PERSONALITIES), run_ieee154_personality),
        ("SUB1G", _filter_433(SUB1GHZ_PERSONALITIES), run_subg_personality),
    ]
    if not args.skip_ook:
        plan.append(("OOK", _filter_433(OOK_PERSONALITIES), run_ook_personality))

    results = []
    for proto, personalities, runner in plan:
        threshold = thresholds[proto]
        print(f"\n--- {proto} (threshold >= {threshold}) ---")
        for p in personalities:
            print(f"\n[ -- ] {proto}: {p.name}")
            (matched, total), exc = run_with_retry(
                runner, args.tx_port, args.rx_port, args.baudrate, p, args.count
            )
            if exc is not None:
                print(f"[FAIL] {p.name}: exception {exc!r}")
                results.append((proto, p.name, 0, 0, False))
                continue
            passed = matched >= threshold
            status = "PASS" if passed else "FAIL"
            print(f"[{status}] {p.name}: matched={matched}/{args.count} total_rx={total}")
            results.append((proto, p.name, matched, total, passed))

    print("\n" + "=" * 60)
    presets_passed = sum(r[4] for r in results)
    total_personalities = len(results)
    print(f"Aggregate: {presets_passed}/{total_personalities} personalities pass")
    return 0 if presets_passed == total_personalities else 1


if __name__ == "__main__":
    sys.exit(main())
