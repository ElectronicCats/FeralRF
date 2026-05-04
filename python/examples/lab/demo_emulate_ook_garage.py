#!/usr/bin/env python3
"""F17 — OOK garage remote emulation demo.

Sends N bursts of a chosen OOK personality (default PT2262_GARAGE_433).
Calls reset_device on exit because OOK locks the radio.
"""
import argparse
import sys

from feralrf.emulation import EV1527_SENSOR_433, HORMANN_GARAGE_868, PT2262_GARAGE_433, emulate_ook
from feralrf.radio import Radio

CHOICES = {
    "pt2262": PT2262_GARAGE_433,
    "ev1527": EV1527_SENSOR_433,
    "hormann": HORMANN_GARAGE_868,
}


def main() -> int:
    parser = argparse.ArgumentParser(description="F17 OOK garage emulation demo")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--personality", default="pt2262", choices=list(CHOICES))
    parser.add_argument("--count", type=int, default=10)
    parser.add_argument("--interval-ms", type=int, default=100)
    parser.add_argument("--power", type=int, default=0)
    args = parser.parse_args()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        radio.init()
        personality = CHOICES[args.personality]
        print(f"Emulating {personality.name} x {args.count}")
        sent = emulate_ook(
            radio,
            personality,
            count=args.count,
            interval_ms=args.interval_ms,
            power_dbm=args.power,
            auto_reset=True,
        )
        print(f"Sent {sent} bursts; radio reset.")
    finally:
        radio.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
