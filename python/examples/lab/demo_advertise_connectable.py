#!/usr/bin/env python3
"""F21 — Demo BLE connectable advertiser. Useful for nRF Connect manual checkpoint."""
import argparse
import sys

from feralrf.radio import Radio


def main() -> int:
    parser = argparse.ArgumentParser(description="F21 BLE connectable advertiser demo")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument(
        "--pdu-type",
        default="ind",
        choices=("ind", "direct", "scan_ind"),
    )
    parser.add_argument("--target-mac", default="DE:AD:BE:EF:CA:FE")
    parser.add_argument("--init-mac", default="11:22:33:44:55:66")
    parser.add_argument("--count", type=int, default=50)
    parser.add_argument("--channel", type=int, default=37)
    parser.add_argument("--power", type=int, default=0)
    parser.add_argument("--mode", default="low", choices=("low", "high"))
    args = parser.parse_args()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        radio.init()
        print(
            f"Advertising {args.pdu_type} as {args.target_mac} on ch{args.channel}; Ctrl-C to stop"
        )
        while True:
            if args.pdu_type == "ind":
                radio.advertise_ind(
                    payload=b"\x02\x01\x06\x09\x09" + b"FERAL_AP",
                    scan_resp_data=b"FERAL_SCAN_RSP",
                    target_addr=args.target_mac,
                    count=args.count,
                    channel=args.channel,
                    power_dbm=args.power,
                )
            elif args.pdu_type == "direct":
                radio.advertise_direct(
                    target_addr=args.target_mac,
                    init_addr=args.init_mac,
                    mode=args.mode,
                    count=args.count,
                    channel=args.channel,
                    power_dbm=args.power,
                )
            else:
                radio.advertise_scan_ind(
                    payload=b"\x02\x01\x06\x09\x09" + b"FERAL_AP",
                    scan_resp_data=b"FERAL_SCAN_RSP",
                    target_addr=args.target_mac,
                    count=args.count,
                    channel=args.channel,
                    power_dbm=args.power,
                )
    except KeyboardInterrupt:
        pass
    finally:
        radio.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
