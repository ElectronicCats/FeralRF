#!/usr/bin/env python3
"""
FeralRF - BLE Sniffer Example

This example demonstrates basic BLE advertising packet sniffing.

Usage:
    python ble_sniffer.py [--port /dev/ttyUSB0]
"""

import argparse
from feralrf import Radio, PHY


def main():
    parser = argparse.ArgumentParser(description='FeralRF BLE Sniffer')
    parser.add_argument('--port', '-p', help='Serial port', default=None)
    parser.add_argument('--channel', '-c', help='BLE channel (37, 38, 39)', type=int, default=37)
    args = parser.parse_args()

    print(f"FeralRF BLE Sniffer")
    print(f"===================")
    print()

    try:
        # Create radio instance
        radio = Radio(port=args.port)

        # Connect and initialize
        print(f"Connecting to device...")
        info = radio.init()
        print(f"Firmware: {info.firmware_version}")
        print(f"Serial: {info.serial}")
        print()

        # Configure for BLE
        print(f"Setting PHY: BLE 1M, Channel {args.channel}")
        radio.set_phy(PHY.BLE_1M, args.channel)

        # Start receiving
        print("Starting RX...")
        radio.start_rx()

        print(f"Listening on BLE channel {args.channel}...")
        print("Press Ctrl+C to stop")
        print()

        packet_count = 0
        try:
            for packet in radio.read_packets(timeout=None):
                packet_count += 1
                print(f"[{packet_count:5d}] "
                      f"RSSI: {packet.rssi_dbm:3d} dBm | "
                      f"CH: {packet.channel:2d} | "
                      f"CRC: {'OK' if packet.crc_ok else 'FAIL'} | "
                      f"Data: {packet.data.hex()}")

        except KeyboardInterrupt:
            print(f"\n\nReceived {packet_count} packets")

        finally:
            print("Stopping RX...")
            radio.stop_rx()
            radio.disconnect()

    except Exception as e:
        print(f"Error: {e}")
        return 1

    return 0


if __name__ == '__main__':
    exit(main())
