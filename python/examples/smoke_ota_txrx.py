#!/usr/bin/env python3
"""OTA TX/RX marker validation between two CatSniffer boards.

Usage:
    python smoke_ota_txrx.py --tx-port /dev/ttyACM3 --rx-port /dev/ttyACM0 \\
        --phy 0 --channel 37
    python smoke_ota_txrx.py --tx-port /dev/ttyACM3 --rx-port /dev/ttyACM0 \\
        --preset gfsk_868_50k

Sends DEADBEEF markers from TX board, verifies they arrive at RX board.
Exits 0 if at least 1 marker received, 1 otherwise.
"""

import argparse
import re
import sys
import time

import serial


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
    time.sleep(2.0)


def main():
    parser = argparse.ArgumentParser(description="OTA TX/RX marker validation")
    parser.add_argument("--tx-port", required=True)
    parser.add_argument("--rx-port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--phy", type=int, default=None)
    parser.add_argument("--channel", type=int, default=0)
    parser.add_argument("--preset", default=None)
    parser.add_argument("--power", type=int, default=0)
    parser.add_argument("--count", type=int, default=10)
    parser.add_argument("--min-markers", type=int, default=1)
    args = parser.parse_args()

    if args.phy is None and args.preset is None:
        print("[FAIL] Must specify --phy or --preset", file=sys.stderr)
        return 2

    from feralrf import PHY, PROP_PRESETS, Radio

    marker = b"\xDE\xAD\xBE\xEF"

    # Reset both
    reset_cc1352(args.tx_port)
    reset_cc1352(args.rx_port)

    tx = Radio(port=args.tx_port, baudrate=args.baudrate)
    rx = Radio(port=args.rx_port, baudrate=args.baudrate)

    try:
        tx.init()
        rx.init()

        if args.preset:
            preset = PROP_PRESETS[args.preset]
            tx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
            rx.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
            tx.configure_prop(**preset)
            rx.configure_prop(**preset)
            label = f"preset={args.preset}"
        else:
            phy = PHY(args.phy)
            tx.set_phy(phy, args.channel)
            rx.set_phy(phy, args.channel)
            label = f"phy={phy.name} ch={args.channel}"

        tx.set_power(args.power)

        rx.start_rx()
        time.sleep(0.3)

        for _ in range(args.count):
            tx.transmit(marker, power_dbm=args.power)
            time.sleep(0.1)

        time.sleep(1.0)

        matched = 0
        total = 0
        for pkt in rx.read_packets(timeout=2.0):
            total += 1
            if marker in pkt.data:
                matched += 1

        try:
            rx.stop_rx()
        except Exception:
            pass

        print(f"[OTA ] {label}: markers={matched}/{args.count} total_rx={total}")

        if matched >= args.min_markers:
            print("[ OK ] OTA PASS")
            return 0
        else:
            print(f"[FAIL] OTA FAIL (need >= {args.min_markers} markers)")
            return 1

    except Exception as e:
        print(f"[FAIL] OTA ERROR: {e}")
        return 4
    finally:
        tx.disconnect()
        rx.disconnect()


if __name__ == "__main__":
    sys.exit(main())
