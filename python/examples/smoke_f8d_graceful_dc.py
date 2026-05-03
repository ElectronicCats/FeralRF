#!/usr/bin/env python3
"""F8d — F2 smoke: graceful disconnect lets peer free its slot immediately.

Validates that connecting → disconnecting → immediately reconnecting
to the SAME peer succeeds in <500 ms. Without F8d, the firmware
silently drops LL_TERMINATE_IND (sleeps in the same task that would
TX it), so the peer falls back to ~1 s supervision timeout and the
second connect either fails or stalls until then.

Pre-conditions:
  - CC1352 board flashed with the F8d firmware (Tasks 3-5 landed).
  - One reachable BLE peripheral.

Pass criteria:
  - First connect succeeds.
  - Disconnect emits RSP_DISCONNECTED with reason 0x16
    (LOCAL_HOST_TERMINATED).
  - Second connect to the same peer succeeds in <500 ms.

Usage:
    source .venv/bin/activate
    python examples/smoke_f8d_graceful_dc.py CB:2B:7D:35:5A:0E 1
    # Default: Soundcore Boom 2 (per F8c live-smoke records).
"""

from __future__ import annotations

import sys
import time

from feralrf import Radio


def parse_mac(mac: str) -> bytes:
    parts = mac.split(":")
    if len(parts) != 6:
        raise SystemExit(f"bad MAC: {mac}")
    return bytes(int(p, 16) for p in reversed(parts))


def main() -> int:
    mac = sys.argv[1] if len(sys.argv) > 1 else "CB:2B:7D:35:5A:0E"
    addr_type = int(sys.argv[2]) if len(sys.argv) > 2 else 1
    addr_le = parse_mac(mac)

    r = Radio()
    r.connect()
    r.init()
    r.reset_device()

    results = {}

    print(f"Cycle 1: connect to {mac} (type={addr_type}) ...")
    res1 = r.ble_connect(addr_le, addr_type=addr_type, timeout=10.0)
    print(f"  result={res1}")
    results["first_connect"] = res1.is_ok
    if not res1.is_ok:
        print("Cannot proceed without a successful first connect.")
        r.disconnect()
        return 1

    time.sleep(0.3)

    print("Disconnect (host-initiated, graceful) ...")
    try:
        r.ble_disconnect(timeout=3.0)
    except Exception as e:
        print(f"  ble_disconnect raised: {e}")

    got_event = next(iter(r.read_disconnect_events(timeout=3.0)), None)
    if got_event is None:
        print("  Disconnect event NOT received")
        results["dc_event"] = False
    else:
        print(f"  Disconnect event: reason=0x{got_event.reason:02X} ({got_event.reason_label})")
        results["dc_event"] = got_event.reason == 0x16

    print(f"Cycle 2: immediate reconnect to {mac} ...")
    t0 = time.monotonic()
    try:
        res2 = r.ble_connect(addr_le, addr_type=addr_type, timeout=2.0)
        elapsed = time.monotonic() - t0
        print(f"  result={res2} elapsed={elapsed:.2f}s")
        results["reconnect_ok"] = res2.is_ok
        results["reconnect_fast"] = res2.is_ok and elapsed < 0.5
    except Exception as e:
        elapsed = time.monotonic() - t0
        print(f"  reconnect raised after {elapsed:.2f}s: {e}")
        results["reconnect_ok"] = False
        results["reconnect_fast"] = False

    try:
        r.ble_disconnect(timeout=3.0)
    except Exception:
        pass

    r.disconnect()

    print()
    for n, ok in results.items():
        print(f"  [{'PASS' if ok else 'FAIL'}]  {n}")

    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    raise SystemExit(main())
