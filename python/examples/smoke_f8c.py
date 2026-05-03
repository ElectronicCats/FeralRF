#!/usr/bin/env python3
"""F8c — live-board smoke for MTU + Read by UUID + DC reason.

Pre-conditions:
  - CC1352 board flashed with the F8c firmware (post-Task 5).
  - One reachable BLE peripheral (advertising) — pass its MAC as argv[1].
  - Address type via argv[2] (0=public, 1=random; default 1).

Pass criteria (printed at end):
  [PASS]  MTU exchange    — peer MTU recorded
  [PASS]  Read by UUID    — at least one entry returned for 0x2A00 (Device Name)
  [PASS]  Disconnect      — host-initiated DC, reason 0x16 received

Usage:
    source .venv/bin/activate
    python examples/smoke_f8c.py AA:BB:CC:DD:EE:FF 1
"""

from __future__ import annotations

import sys
import time

from feralrf import Radio


def parse_mac(mac: str) -> bytes:
    parts = mac.split(":")
    if len(parts) != 6:
        raise SystemExit(f"bad MAC: {mac}")
    return bytes(int(p, 16) for p in reversed(parts))  # LE


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    addr_le = parse_mac(sys.argv[1])
    addr_type = int(sys.argv[2]) if len(sys.argv) >= 3 else 1

    r = Radio()
    r.connect()
    r.init()
    r.reset_device()

    results = {}

    print(f"Connecting to {sys.argv[1]} (type={addr_type}) ...")
    res = r.ble_connect(addr_le, addr_type=addr_type, timeout=10.0)
    if not res.is_ok:
        print(f"  ble_connect failed: {res}")
        return 1

    # Allow the link to settle.
    time.sleep(0.5)

    # 1) MTU exchange
    try:
        peer_mtu = r.gatt_exchange_mtu(client_mtu=23, timeout=5.0)
        print(f"  MTU exchange: peer reports {peer_mtu}")
        results["mtu"] = peer_mtu >= 23
    except Exception as e:
        print(f"  MTU exchange FAILED: {e}")
        results["mtu"] = False

    # 2) Read by UUID — Device Name (0x2A00)
    try:
        attrs = r.gatt_read_by_uuid(uuid=0x2A00, timeout=5.0)
        print(f"  Read by UUID 0x2A00: {len(attrs)} entry/entries")
        for a in attrs:
            try:
                name = a.value.decode("utf-8", errors="replace")
            except Exception:
                name = a.value.hex()
            print(f"    handle=0x{a.handle:04X}  value={name!r}")
        results["read_by_uuid"] = len(attrs) >= 1
    except Exception as e:
        print(f"  Read by UUID FAILED: {e}")
        results["read_by_uuid"] = False

    # 3) Disconnect — host-initiated, expect reason 0x16
    print("  Disconnecting (host-initiated) ...")
    try:
        r.ble_disconnect(timeout=3.0)
    except Exception as e:
        print(f"  ble_disconnect raised: {e}")

    got_event = None
    for ev in r.read_disconnect_events(timeout=3.0):
        got_event = ev
        break
    if got_event is None:
        print("  Disconnect event: NOT received")
        results["disconnect"] = False
    else:
        print(f"  Disconnect event: reason=0x{got_event.reason:02X} ({got_event.reason_label})")
        results["disconnect"] = got_event.reason == 0x16

    r.disconnect()

    print()
    for name, ok in results.items():
        print(f"  [{'PASS' if ok else 'FAIL'}]  {name}")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
