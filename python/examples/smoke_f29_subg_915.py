#!/usr/bin/env python3
"""F29 vuelta 1 — Sub-G 915 MHz preset smoke (Sidewalk + Wi-SUN baseline).

Recorre los 3 presets F29 (sidewalk_915_fsk_50k, sidewalk_915_fsk_250k,
wisun_915_fsk_50k) y para cada uno: TX board envía N markers OTA, RX board
cuenta. Pass total = (count × 3) markers; per-preset pass = >= count markers.

Usage:
    python smoke_f29_subg_915.py --tx-port /dev/ttyACM5 --rx-port /dev/ttyACM2
    python smoke_f29_subg_915.py --tx-port /dev/ttyACM5 --rx-port /dev/ttyACM2 --count 10
"""

import argparse
import re
import sys
import time

import serial

F29_PRESETS = ("sidewalk_915_fsk_50k", "sidewalk_915_fsk_250k", "wisun_915_fsk_50k")


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


def run_preset(
    tx_port: str,
    rx_port: str,
    baudrate: int,
    preset_name: str,
    count: int,
    power: int,
    rx_window: float,
) -> tuple[int, int]:
    """Run a single preset OTA round-trip. Returns (matched, total_rx)."""
    from feralrf import PHY, PROP_PRESETS, Radio, RxStreamError

    marker = b"\xde\xad\xbe\xef"
    preset = PROP_PRESETS[preset_name]

    reset_cc1352(tx_port)
    reset_cc1352(rx_port)

    tx = Radio(port=tx_port, baudrate=baudrate)
    rx = Radio(port=rx_port, baudrate=baudrate)

    try:
        tx.init()
        rx.init()

        tx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
        rx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
        tx.configure_prop(**preset)
        rx.configure_prop(**preset)
        tx.set_power(power)

        rx.start_rx()
        time.sleep(0.3)

        for _ in range(count):
            tx.transmit(marker, power_dbm=power)
            time.sleep(0.1)

        time.sleep(1.0)

        matched = 0
        total = 0
        for pkt in rx.read_packets(timeout=rx_window):
            if isinstance(pkt, RxStreamError):
                continue
            total += 1
            # F29 vuelta 1: rigorous validation — require CRC pass AND marker
            # bytes match. Without crc_ok, a corrupted packet whose bytes
            # happen to contain the 4-byte marker would count (vanishingly
            # unlikely at ~1/4.3B but explicit is safer than implicit).
            if pkt.crc_ok and marker in pkt.data:
                matched += 1

        try:
            rx.stop_rx()
        except Exception:
            pass

        return matched, total
    finally:
        tx.disconnect()
        rx.disconnect()


def main() -> int:
    parser = argparse.ArgumentParser(description="F29 Sub-G 915 MHz preset smoke")
    parser.add_argument("--tx-port", required=True, help="TX board (no Sub-1GHz fault)")
    parser.add_argument("--rx-port", required=True, help="RX board")
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--count", type=int, default=10, help="Markers per preset")
    parser.add_argument("--min-markers", type=int, default=10, help="Min markers/preset for PASS")
    parser.add_argument("--power", type=int, default=0)
    parser.add_argument("--rx-window", type=float, default=2.0)
    args = parser.parse_args()

    print("F29 vuelta 1 — Sub-G 915 MHz preset smoke")
    print(f"TX={args.tx_port} RX={args.rx_port} count={args.count} power={args.power} dBm")
    print("=" * 60)

    results = []
    for name in F29_PRESETS:
        print(f"\n[ -- ] {name}")
        try:
            matched, total = run_preset(
                args.tx_port,
                args.rx_port,
                args.baudrate,
                name,
                args.count,
                args.power,
                args.rx_window,
            )
        except Exception as exc:
            print(f"[FAIL] {name}: exception {exc!r}")
            results.append((name, 0, 0, False))
            continue

        passed = matched >= args.min_markers
        status = "PASS" if passed else "FAIL"
        print(f"[{status}] {name}: markers={matched}/{args.count} total_rx={total}")
        results.append((name, matched, total, passed))

    print("\n" + "=" * 60)
    total_matched = sum(r[1] for r in results)
    expected = args.count * len(F29_PRESETS)
    all_pass = all(r[3] for r in results)
    presets_passed = sum(r[3] for r in results)
    print(
        f"Aggregate: {total_matched}/{expected} markers, {presets_passed}/{len(results)} presets pass"
    )

    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
