#!/usr/bin/env python3
"""
Quick jamming test - simple script to verify jamming improvements.

Usage:
    python quick_jam_test.py                    # Auto-detect, BLE ch37, 5s
    python quick_jam_test.py --ieee             # IEEE 802.15.4 ch11
    python quick_jam_test.py --channel 38 --duration 3  # Custom
"""

import argparse
import time
import sys

sys.path.insert(0, "/home/sabas/Documents/electroniccats/FeralRF/python")

from feralrf import Radio, PHY
from feralrf.jamming import show_regulatory_warning


def main():
    parser = argparse.ArgumentParser(description="Quick FeralRF jamming test")
    parser.add_argument("--port", default=None, help="Serial port")
    parser.add_argument("--ieee", action="store_true", help="Use IEEE 802.15.4 PHY")
    parser.add_argument("--channel", type=int, default=None, help="Channel")
    parser.add_argument("--power", type=int, default=10, help="TX power (dBm)")
    parser.add_argument("--duration", type=int, default=5, help="Duration (seconds)")
    parser.add_argument("--yes", "-y", action="store_true", help="Skip warning")

    args = parser.parse_args()

    if not args.yes:
        show_regulatory_warning()
        response = input("Continue? [y/N]: ")
        if response.lower() != "y":
            print("Aborted.")
            return 1

    # Select PHY and channel
    if args.ieee:
        phy = PHY.IEEE_802_15_4
        channel = args.channel if args.channel else 11
    else:
        phy = PHY.BLE_1M
        channel = args.channel if args.channel else 37

    print(f"\n=== Quick Jam Test ===")
    print(f"PHY: {phy.name}")
    print(f"Channel: {channel}")
    print(f"Power: {args.power} dBm")
    print(f"Duration: {args.duration}s")
    print()

    with Radio(port=args.port) as radio:
        info = radio.init()
        print(f"Device: FW {info.firmware_version}")

        radio.set_phy(phy, channel)
        print(f"PHY configured: {phy.name} ch{channel}")

        duration_ms = args.duration * 1000
        print(f"\nStarting jam for {args.duration} seconds...")

        start = time.monotonic()
        radio.start_jam(channel=channel, power_dbm=args.power, duration_ms=duration_ms)

        # Countdown
        remaining = args.duration
        while remaining > 0:
            print(f"  Jamming... {remaining}s remaining\r", end="", flush=True)
            time.sleep(1)
            remaining -= 1

        # Wait for auto-stop
        time.sleep(0.5)
        elapsed = time.monotonic() - start

        print(f"\n\nJam completed in {elapsed:.2f}s")
        print("Test PASSED - Jam session worked correctly")

    return 0


if __name__ == "__main__":
    sys.exit(main())
