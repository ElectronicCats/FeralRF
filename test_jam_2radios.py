#!/usr/bin/env python3
"""
Test jamming effectiveness with 2 radios.

Radio A (Jammer): FeralRF running jam session
Radio B (Monitor): FeralRF or any BLE/IEEE sniffer

Usage:
    # Terminal 1 - Monitor (sniffer)
    python test_jam_2radios.py --role monitor --port /dev/ttyACM1

    # Terminal 2 - Jammer
    python test_jam_2radios.py --role jammer --port /dev/ttyACM0 -y
"""

import argparse
import time
import sys

sys.path.insert(0, "/home/sabas/Documents/electroniccats/FeralRF/python")

from feralrf import Radio, PHY
from feralrf.jamming import show_regulatory_warning


def run_jammer(port: str, phy: PHY, channel: int, power: int, duration: int):
    """Run jammer - transmits continuously"""
    print(f"[JAMMER] Starting on {port}")
    print(f"[JAMMER] PHY: {phy.name}, Channel: {channel}, Power: {power}dBm")

    with Radio(port=port) as radio:
        info = radio.init()
        print(f"[JAMMER] Connected: FW {info.firmware_version}")

        radio.set_phy(phy, channel)

        print(f"[JAMMER] Starting jam for {duration}s...")
        print("[JAMMER] Watch the MONITOR terminal for packet loss!")

        radio.start_jam(channel=channel, power_dbm=power, duration_ms=duration * 1000)

        # Countdown
        for i in range(duration, 0, -1):
            print(f"[JAMMER] Jamming... {i}s remaining")
            time.sleep(1)

        print("[JAMMER] Jam session ended")


def run_monitor(port: str, phy: PHY, channel: int, duration: int):
    """Run monitor - counts received packets"""
    print(f"[MONITOR] Starting on {port}")
    print(f"[MONITOR] PHY: {phy.name}, Channel: {channel}")

    with Radio(port=port) as radio:
        info = radio.init()
        print(f"[MONITOR] Connected: FW {info.firmware_version}")

        radio.set_phy(phy, channel)
        radio.start_rx()
        print(f"[MONITOR] Listening for {duration + 2}s...")
        print("[MONITOR] Start the JAMMER now!")
        print()

        # Count packets
        packet_count = 0
        start_time = time.monotonic()
        end_time = start_time + duration + 2

        # Stats every second
        last_stats = start_time
        packets_per_second = []

        while time.monotonic() < end_time:
            try:
                for pkt in radio.read_packets(timeout=0.5):
                    packet_count += 1

                now = time.monotonic()
                if now - last_stats >= 1.0:
                    elapsed = now - start_time
                    pps = packet_count / elapsed if elapsed > 0 else 0
                    packets_per_second.append(packet_count)
                    print(f"[MONITOR] t={int(elapsed)}s: {packet_count} total packets ({pps:.1f} pkt/s)")
                    last_stats = now

            except Exception:
                pass

        radio.stop_rx()

        # Summary
        print()
        print("=" * 50)
        print("[MONITOR] RESULTS")
        print("=" * 50)
        print(f"Total packets received: {packet_count}")
        print(f"Duration: {duration + 2}s")
        print(f"Average: {packet_count / (duration + 2):.1f} pkt/s")

        if len(packets_per_second) >= 2:
            print()
            print("If jamming was effective, you should see:")
            print("  - Packets at start (before jam)")
            print("  - Drop to ~0 during jam")
            print("  - Packets resume after jam ends")


def main():
    parser = argparse.ArgumentParser(description="2-radio jamming test")
    parser.add_argument("--role", choices=["jammer", "monitor"], required=True)
    parser.add_argument("--port", required=True, help="Serial port")
    parser.add_argument("--phy", choices=["ble", "ieee"], default="ble")
    parser.add_argument("--channel", type=int, default=37)
    parser.add_argument("--power", type=int, default=20)
    parser.add_argument("--duration", type=int, default=10)
    parser.add_argument("-y", action="store_true", help="Skip warning (jammer only)")

    args = parser.parse_args()

    phy = PHY.BLE_1M if args.phy == "ble" else PHY.IEEE_802_15_4
    channel = args.channel if args.channel else (11 if args.phy == "ieee" else 37)

    if args.role == "jammer":
        if not args.y:
            show_regulatory_warning()
            response = input("Continue? [y/N]: ")
            if response.lower() != "y":
                return 1
        run_jammer(args.port, phy, channel, args.power, args.duration)
    else:
        run_monitor(args.port, phy, channel, args.duration)

    return 0


if __name__ == "__main__":
    sys.exit(main())
