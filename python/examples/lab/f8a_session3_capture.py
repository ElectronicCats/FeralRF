"""F8A Session 3 — capture one connection attempt.

Runs CMD_CONNECT against CH573, lets the firmware burn through master
events until supervisionTimeout drops the link, then dumps:

    {
      "conn_result": int,
      "conn_status": <ConnectionStatus.__dict__ snapshot>,
      "debug_timing": [<DebugTimingEntry.__dict__>...],
      "wallclock_capture_start_unix_ns": int,
      "wallclock_capture_end_unix_ns":   int,
    }

The wallclock fields anchor the capture to the same UNIX time base as
the Sniffle pcap (which records pcap-standard ns timestamps).
"""

import argparse
import dataclasses
import json
import time
from pathlib import Path

from feralrf import Radio


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyACM8")
    parser.add_argument("--target", default="DC:32:62:8D:E1:09")
    parser.add_argument("--addr-type", type=int, default=0)
    parser.add_argument("--out", required=True)
    parser.add_argument(
        "--linger",
        type=float,
        default=2.5,
        help="Seconds to wait after CONN_RESULT before dumping telemetry.",
    )
    args = parser.parse_args()

    r = Radio(args.port)
    r.connect()
    r.init()

    addr_le = bytes(reversed(bytes.fromhex(args.target.replace(":", ""))))
    t0 = time.time_ns()
    res = r.ble_connect(addr_le, addr_type=args.addr_type)
    time.sleep(args.linger)
    status = r.conn_status()
    timing = r.debug_timing()
    t1 = time.time_ns()

    try:
        r.ble_disconnect()
    except Exception:
        pass
    r.disconnect()

    out = {
        "conn_result": int(res.result),
        "conn_status": dataclasses.asdict(status),
        "debug_timing": [dataclasses.asdict(e) for e in timing.entries],
        "wallclock_capture_start_unix_ns": t0,
        "wallclock_capture_end_unix_ns": t1,
    }
    Path(args.out).write_text(json.dumps(out, indent=2))
    print(
        f"wrote {args.out}: result={res.result} events={status.events} "
        f"timing_count={timing.count}"
    )


if __name__ == "__main__":
    main()
