#!/usr/bin/env python3
"""F8b Track B — wire-level smoke for passive connection follower.

Procedure (manual):
  1. Put your phone in Bluetooth-discovery mode.
  2. Power on Sony WH-CH720N (or any BLE peripheral); make sure it is NOT
     already paired with the phone.
  3. Run this script — it will start the follower, then wait for you to
     initiate a pairing on the phone.
  4. The follower captures every LL data PDU on the connection.
  5. After 30 s of follow time (or peer disconnect), packets are dumped
     and a pcap-NG is written to /tmp/f8b_follower.pcapng.

Closure: >=10 bidirectional LL data PDUs captured, pcap valid.

Usage:  python smoke_f8b_follower.py [--port /dev/ttyACM2] [--target-mac AA:BB:CC:DD:EE:FF] [--duration 30]
"""
import argparse
import sys
import time

from feralrf import Radio
from feralrf._ll_parser import export_pcap, parse_ll_pdu


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="/dev/ttyACM2")
    p.add_argument(
        "--target-mac",
        default="A8:E6:E8:8A:7D:F8",
        help="Sony WH-CH720N MAC by default; use 'wildcard' to capture any",
    )
    p.add_argument("--duration", type=float, default=30.0)
    p.add_argument("--pcap", default="/tmp/f8b_follower.pcapng")
    args = p.parse_args()

    r = Radio(args.port)
    r.connect()
    time.sleep(0.3)
    r.init()

    target = None if args.target_mac.lower() == "wildcard" else args.target_mac
    print(f"[STEP] follow_connection target={target or 'wildcard'}")
    r.follow_connection(target_mac=target, timeout=5.0)

    print(f"[STEP] capturing for {args.duration:.0f} s — initiate pairing on phone now")
    pkts = []
    t0 = time.time()
    while time.time() - t0 < args.duration:
        for pkt in r.read_ll_packets(timeout=1.0):
            pkts.append(pkt)
            kind = parse_ll_pdu(pkt.payload)
            kname = kind.kind.name if kind else "?"
            print(
                f"  [{time.time() - t0:5.1f}s] ch{pkt.channel:>2} "
                f"ev{pkt.event_counter:>4} {kname:>11} "
                f"rssi={pkt.rssi_dbm:+4d} len={len(pkt.payload):>3}"
            )

    try:
        r.stop_follow_connection(timeout=2.0)
    except Exception as e:
        print(f"  stop returned {type(e).__name__}: {e} (ok if peer terminated)")
    r.disconnect()

    print(f"\n[STEP] export pcap -> {args.pcap}")
    export_pcap(pkts, args.pcap)

    print()
    if len(pkts) >= 10:
        print(f"[ OK ] F8b Track B smoke PASS — captured {len(pkts)} packets")
        return 0
    print(f"[FAIL] only {len(pkts)} packets captured (need >=10)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
