#!/usr/bin/env python3
"""Deterministic repro: ble_connect → gatt_discover → ble_disconnect →
ble_connect → gatt_discover. Second discover fails with timeout per
memory/project_gatt_attclient_bug.md.

Usage: python diag_attclient_repro.py [port] [target_mac]
"""
import sys
import time

from feralrf import Radio

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM2"
target_mac = sys.argv[2] if len(sys.argv) > 2 else "A8:E6:E8:8A:7D:F8"
addr_le = bytes.fromhex("".join(target_mac.split(":")[::-1]))

r = Radio(port)
r.connect()
time.sleep(0.3)
r.init()

for cycle in range(1, 6):
    print(f"\n=== cycle {cycle}/5 ===")
    print("  ble_connect")
    res = r.ble_connect(addr_le, addr_type=0, timeout=10.0)
    print(f"    result code = {res.result}")
    if res.result != 0:
        print("    CONNECT FAILED — aborting cycle")
        break
    time.sleep(0.5)
    print("  gatt_discover")
    try:
        d = r.gatt_discover(timeout=15.0)
        print(f"    OK — services={len(d.services)} chars={len(d.characteristics)}")
    except Exception as e:
        print(f"    FAILED: {type(e).__name__}: {e}")
    print("  ble_disconnect")
    try:
        r.ble_disconnect()
    except Exception:
        pass
    time.sleep(1.0)

r.disconnect()
