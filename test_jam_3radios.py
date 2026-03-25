#!/usr/bin/env python3
"""
3-Radio Jamming Effectiveness Test

Setup:
  [TX] Transmisor --packets--> [RX] Monitor
           ^ (interference)
        [JAM] Jammer

Expected result: Monitor receives 0 packets from TX during active jam.

Usage (3 terminals):

  Terminal 1 - Transmitter (sends known packets):
    python test_jam_3radios.py --role tx --port /dev/ttyACM0

  Terminal 2 - Monitor (counts received packets):
    python test_jam_3radios.py --role rx --port /dev/ttyACM1

  Terminal 3 - Jammer:
    python test_jam_3radios.py --role jam --port /dev/ttyACM2 -y
"""

import argparse
import time
import sys
import struct

sys.path.insert(0, "/home/sabas/Documents/electroniccats/FeralRF/python")

from feralrf import Radio, PHY
from feralrf.jamming import show_regulatory_warning

# Marker to identify our test packets
TEST_PACKET_MARKER = b"\xDE\xAD\xBE\xEFTEST"
TEST_PACKET_DATA = TEST_PACKET_MARKER + b"\x00" * 20  # 24 bytes total


def run_transmitter(port: str, channel: int, duration: int):
    """Transmitter - sends test packets continuously"""
    print(f"[TX] Starting transmitter on {port}")
    print(f"[TX] Channel: {channel}, Duration: {duration}s")
    print(f"[TX] Packet marker: {TEST_PACKET_MARKER.hex()}")

    packet_count = 0

    with Radio(port=port) as radio:
        info = radio.init()
        print(f"[TX] Connected: FW {info.firmware_version}")

        radio.set_phy(PHY.BLE_1M, channel)

        end_time = time.monotonic() + duration
        while time.monotonic() < end_time:
            try:
                # Send test packet
                radio.transmit_frame(TEST_PACKET_DATA)
                packet_count += 1

                # Status every 10 packets
                if packet_count % 10 == 0:
                    remaining = int(end_time - time.monotonic())
                    print(f"[TX] Sent {packet_count} packets, {remaining}s remaining")

                time.sleep(0.05)  # ~20 packets/second

            except Exception as e:
                print(f"[TX] Error: {e}")
                time.sleep(0.1)

        print(f"[TX] Done. Total sent: {packet_count}")


def run_monitor(port: str, channel: int, duration: int):
    """Monitor - counts received test packets"""
    print(f"[RX] Starting monitor on {port}")
    print(f"[RX] Channel: {channel}, Duration: {duration}s")
    print(f"[RX] Looking for marker: {TEST_PACKET_MARKER.hex()}")

    total_packets = 0
    test_packets = 0
    phases = {"before": 0, "during": 0, "after": 0}
    current_phase = "before"
    phase_start = time.monotonic()

    with Radio(port=port) as radio:
        info = radio.init()
        print(f"[RX] Connected: FW {info.firmware_version}")

        radio.set_phy(PHY.BLE_1M, channel)
        radio.start_rx()
        print(f"[RX] Listening... Start TX and JAM in other terminals!")
        print()

        end_time = time.monotonic() + duration
        last_print = time.monotonic()

        while time.monotonic() < end_time:
            try:
                for pkt in radio.read_packets(timeout=0.5):
                    total_packets += 1

                    # Check if it's our test packet
                    if pkt.data.startswith(TEST_PACKET_MARKER):
                        test_packets += 1
                        phases[current_phase] += 1

                now = time.monotonic()
                if now - last_print >= 1.0:
                    elapsed = int(now - phase_start)
                    print(f"[RX] t={elapsed:3d}s | Phase: {current_phase:6s} | "
                          f"Test packets: {test_packets:4d} | Total: {total_packets:4d}")
                    last_print = now

            except Exception:
                pass

            # Phase detection based on time (rough)
            elapsed = time.monotonic() - phase_start
            if elapsed > 5 and current_phase == "before":
                current_phase = "during"
                print(f"[RX] >>> Phase change: BEFORE -> DURING (jam should be active)")
            elif elapsed > 15 and current_phase == "during":
                current_phase = "after"
                print(f"[RX] >>> Phase change: DURING -> AFTER (jam should be stopped)")

        radio.stop_rx()

        # Results
        print()
        print("=" * 60)
        print("[RX] JAMMING EFFECTIVENESS RESULTS")
        print("=" * 60)
        print(f"Test packets received:")
        print(f"  BEFORE jam: {phases['before']:4d}")
        print(f"  DURING jam: {phases['during']:4d}")
        print(f"  AFTER jam:  {phases['after']:4d}")
        print(f"  TOTAL:      {test_packets:4d}")
        print()

        if phases['before'] > 0:
            effectiveness = 100 - (phases['during'] / phases['before'] * 100)
            print(f"JAMMING EFFECTIVENESS: {effectiveness:.1f}%")
            print(f"  (0% = no effect, 100% = complete blocking)")
        else:
            print("WARNING: No test packets received before jam!")
            print("  Check that TX is running on correct channel.")


def run_jammer(port: str, channel: int, power: int):
    """Jammer - activates jamming after 5s delay"""
    print(f"[JAM] Starting jammer on {port}")
    print(f"[JAM] Channel: {channel}, Power: {power}dBm")
    print(f"[JAM] Will start jamming in 5 seconds...")

    with Radio(port=port) as radio:
        info = radio.init()
        print(f"[JAM] Connected: FW {info.firmware_version}")

        radio.set_phy(PHY.BLE_1M, channel)

        # Wait before starting
        print("[JAM] Waiting 5s for TX and RX to start...")
        for i in range(5, 0, -1):
            print(f"[JAM] {i}...")
            time.sleep(1)

        print("[JAM] >>> STARTING JAM (10 seconds) <<<")
        radio.start_jam(channel=channel, power_dbm=power, duration_ms=10000)

        for i in range(10, 0, -1):
            print(f"[JAM] Jamming... {i}s remaining")
            time.sleep(1)

        print("[JAM] >>> JAM STOPPED <<<")
        print("[JAM] Waiting for test to complete...")
        time.sleep(5)
        print("[JAM] Done")


def main():
    parser = argparse.ArgumentParser(description="3-radio jamming effectiveness test")
    parser.add_argument("--role", choices=["tx", "rx", "jam"], required=True,
                        help="Role: tx=transmitter, rx=monitor, jam=jammer")
    parser.add_argument("--port", required=True, help="Serial port")
    parser.add_argument("--channel", type=int, default=37)
    parser.add_argument("--power", type=int, default=20)
    parser.add_argument("--duration", type=int, default=25)
    parser.add_argument("-y", action="store_true", help="Skip warning (jammer only)")

    args = parser.parse_args()

    if args.role == "jam" and not args.y:
        show_regulatory_warning()
        response = input("Continue? [y/N]: ")
        if response.lower() != "y":
            return 1

    if args.role == "tx":
        run_transmitter(args.port, args.channel, args.duration)
    elif args.role == "rx":
        run_monitor(args.port, args.channel, args.duration)
    else:
        run_jammer(args.port, args.channel, args.power)

    return 0


if __name__ == "__main__":
    sys.exit(main())
