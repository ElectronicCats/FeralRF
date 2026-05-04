#!/usr/bin/env python3
"""F17 — Sub-1GHz sensor emulation demo.

Loops a chosen personality (default GFSK_868_SENSOR) at 1 Hz until Ctrl-C.
Wraps the M2 burst API (count=1 per call).
"""
import argparse
import sys
import time

from feralrf.emulation import GFSK_433_SENSOR, GFSK_868_SENSOR, WMBUS_T1_METER, emulate_sub1ghz
from feralrf.radio import Radio

CHOICES = {
    "gfsk_868": GFSK_868_SENSOR,
    "gfsk_433": GFSK_433_SENSOR,
    "wmbus": WMBUS_T1_METER,
}


def main() -> int:
    parser = argparse.ArgumentParser(description="F17 Sub-1GHz sensor emulation demo")
    parser.add_argument("--port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--personality", default="gfsk_868", choices=list(CHOICES))
    parser.add_argument("--interval-s", type=float, default=1.0, help="Loop interval in seconds")
    parser.add_argument("--power", type=int, default=0)
    args = parser.parse_args()

    radio = Radio(port=args.port, baudrate=args.baudrate)
    try:
        radio.init()
        personality = CHOICES[args.personality]
        print(f"Emulating {personality.name} at {args.interval_s}s interval; Ctrl-C to stop")
        while True:
            emulate_sub1ghz(radio, personality, count=1, interval_ms=0, power_dbm=args.power)
            time.sleep(args.interval_s)
    except KeyboardInterrupt:
        pass
    finally:
        radio.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
