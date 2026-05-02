#!/usr/bin/env python3
"""F8b Track A — wire-level smoke for GATT notifications.

Connects to Sony WH-CH720N, discovers, subscribes to a panel of
custom Sony characteristics, and waits 30s for the user to press
buttons (NC toggle, play/pause, etc.). Closure: at least one
notification captured on any subscribed handle.

Usage: python smoke_f8b_notifications.py [port] [target_mac]
"""
import argparse
import sys
import time

from feralrf import Radio

# Sony custom service notification handles per
# docs/investigations/2026-05-01-sony-wh-ch720n.md
SONY_NOTIFY_HANDLES = (170, 186, 194, 212, 564, 580, 612)


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="/dev/ttyACM2")
    p.add_argument("--target-mac", default="A8:E6:E8:8A:7D:F8")
    p.add_argument("--duration", type=float, default=30.0)
    args = p.parse_args()

    addr_le = bytes.fromhex("".join(args.target_mac.split(":")[::-1]))
    r = Radio(args.port)
    r.connect()
    time.sleep(0.3)
    r.init()

    print(f"[STEP] connect {args.target_mac}")
    res = r.ble_connect(addr_le, addr_type=0, timeout=10.0)
    if res.result != 0:
        print(f"  CONNECT FAILED code={res.result}")
        return 1
    time.sleep(0.5)

    print("[STEP] gatt_discover")
    disc = r.gatt_discover(timeout=15.0)
    print(f"  services={len(disc.services)} chars={len(disc.characteristics)}")

    print(f"[STEP] subscribe to {len(SONY_NOTIFY_HANDLES)} candidate handles")
    subscribed = []
    for h in SONY_NOTIFY_HANDLES:
        try:
            r.gatt_subscribe(handle=h, enable=True)
            subscribed.append(h)
            print(f"  h{h:>3}  OK")
        except Exception as e:
            print(f"  h{h:>3}  FAIL ({type(e).__name__})")

    if not subscribed:
        print("[FAIL] no handle subscribable")
        r.ble_disconnect()
        r.disconnect()
        return 1

    print(f"[STEP] waiting {args.duration:.0f}s — press buttons on the headphones now")
    t0 = time.time()
    notifs: list = []
    while time.time() - t0 < args.duration:
        for n in r.read_gatt_notifications(timeout=1.0):
            notifs.append(n)
            print(f"  [{time.time() - t0:5.1f}s] h{n.handle}: {n.value.hex()}")

    r.ble_disconnect()
    r.disconnect()

    print()
    if notifs:
        print(f"[ OK ] F8b Track A smoke PASS — captured {len(notifs)} notifications")
        return 0
    print(
        "[FAIL] no notifications captured — try pressing more buttons or check that the headphones are awake"
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
