#!/usr/bin/env python3
"""
FeralRF - BLE Sniffer Example

This example demonstrates basic BLE advertising packet sniffing.

Usage:
    python ble_sniffer.py [--port /dev/ttyUSB0]
"""

import argparse
from typing import Optional

from feralrf import PHY, Radio, RxStreamError


def ll_kind_label(kind: Optional[int]) -> str:
    if kind == 1:
        return "ADV"
    if kind == 2:
        return "SCAN"
    if kind == 3:
        return "CONNECT"
    if kind == 4:
        return "DATA"
    return "N/A"


def ll_subtype_label(kind: Optional[int], pdu_type: Optional[int], flags: Optional[int]) -> str:
    if kind is None or pdu_type is None:
        return "-"
    if flags is not None and (flags & 0x08):
        return "RESERVED"
    if kind == 1:
        mapping = {
            0x00: "ADV_IND",
            0x01: "ADV_DIRECT_IND",
            0x02: "ADV_NONCONN_IND",
            0x06: "ADV_SCAN_IND",
            0x07: "ADV_EXT_IND",
        }
        return mapping.get(pdu_type, f"ADV_0x{pdu_type:02X}")
    if kind == 2:
        mapping = {0x03: "SCAN_REQ", 0x04: "SCAN_RSP"}
        return mapping.get(pdu_type, f"SCAN_0x{pdu_type:02X}")
    if kind == 3:
        return "CONNECT_IND"
    if kind == 4:
        mapping = {0x01: "DATA_CONT", 0x02: "DATA_START", 0x03: "CTRL"}
        return mapping.get(pdu_type, f"DATA_0x{pdu_type:02X}")
    return f"0x{pdu_type:02X}"


def main():
    parser = argparse.ArgumentParser(description="FeralRF BLE Sniffer")
    parser.add_argument("--port", "-p", help="Serial port", default=None)
    parser.add_argument("--channel", "-c", help="BLE channel (37, 38, 39)", type=int, default=37)
    args = parser.parse_args()

    print("FeralRF BLE Sniffer")
    print("===================")
    print()

    try:
        # Create radio instance
        radio = Radio(port=args.port)

        # Connect and initialize
        print("Connecting to device...")
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
                # F8f #7b: read_packets now also yields RxStreamError; skip those.
                if isinstance(packet, RxStreamError):
                    continue
                packet_count += 1
                print(
                    f"[{packet_count:5d}] "
                    f"RSSI: {packet.rssi_dbm:3d} dBm | "
                    f"CH: {packet.channel:2d} | "
                    f"CRC: {'OK' if packet.crc_ok else 'FAIL'} | "
                    f"LL: {ll_kind_label(packet.ll_pdu_kind)}"
                    f"/{ll_subtype_label(packet.ll_pdu_kind, packet.ll_pdu_type, packet.ll_pdu_flags)} | "
                    f"Data: {packet.data.hex()}"
                )

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


if __name__ == "__main__":
    exit(main())
