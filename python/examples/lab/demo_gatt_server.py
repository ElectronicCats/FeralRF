#!/usr/bin/env python3
"""F20.a.1 — single-board peripheral demo for nRF Connect manual testing."""
import argparse
import sys

from feralrf.radio import Radio


def main() -> int:
    parser = argparse.ArgumentParser(description="F20.a.1 GATT server demo (nRF Connect target)")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--target-mac", default="DE:AD:BE:EF:CA:FE")
    parser.add_argument("--count", type=int, default=2000)
    args = parser.parse_args()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        radio.init()
        radio.serve_gatt()
        print(f"Advertising as {args.target_mac}, GATT server T2 (FERAL_GATT)")
        print("Connect with nRF Connect; read handle 3 (Device Name) and handle 6 (Test).")
        radio.advertise_ind(
            payload=b"\x02\x01\x06",
            scan_resp_data=b"FERAL_GATT_SR",
            target_addr=args.target_mac,
            count=args.count,
            interval_us=10000,
        )
    except KeyboardInterrupt:
        pass
    finally:
        radio.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
