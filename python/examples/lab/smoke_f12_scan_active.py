#!/usr/bin/env python3
"""F12 wire-level smoke — active BLE scan against ambient lab traffic.

Closure criterion: ≥1 device with name + UUIDs/mfg + scan_rsp_count > 0.
If lab is RF-quiet or has no scannable peripheral in range, smoke fails
and reports — bring an ESP32/phone/smart-bulb closer and retry.

Usage:
    python smoke_f12_scan_active.py [--port /dev/ttyACM8] [--duration 10]
"""

import argparse
import sys
import time
import warnings

from feralrf import Radio

warnings.simplefilter("ignore")


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="/dev/ttyACM8")
    p.add_argument("--duration", type=float, default=10.0)
    args = p.parse_args()

    r = Radio(args.port)
    r.connect()
    time.sleep(0.3)
    r.init()

    print(f"[STEP] active scan {args.duration}s on ch 37/38/39 from {args.port}")
    t0 = time.time()
    result = r.scan_ble_active(duration=args.duration)
    dt = time.time() - t0

    n_devices = len(result)
    n_with_rsp = sum(1 for x in result.values() if x.scan_rsp_count > 0)
    closure_eligible = [
        x
        for x in result.values()
        if x.scan_rsp_count > 0
        and x.name
        and (x.uuids_16bit or x.uuids_128bit or x.manufacturer_data)
    ]

    print(
        f"[INFO] devices={n_devices}, scan_rsps={n_with_rsp}, "
        f"closure-eligible={len(closure_eligible)}, dt={dt:.1f}s"
    )

    r.disconnect()

    if n_devices < 3:
        print(f"[FAIL] expected ≥3 BLE devices in lab ambient; got {n_devices}")
        return 1
    if n_with_rsp < 1:
        print(
            "[FAIL] expected ≥1 device responding to SCAN_REQ; bring a scannable peripheral closer"
        )
        return 1
    if not closure_eligible:
        print("[FAIL] F12 closure criterion not met: no device with name + UUIDs/mfg + scan_rsp")
        return 1

    print("[ OK ] F12 wire smoke PASS")
    for x in closure_eligible[:3]:
        uuids_total = len(x.uuids_16bit) + len(x.uuids_128bit)
        mfg_total = len(x.manufacturer_data)
        print(
            f"        {x.mac} '{x.name}' adv={x.adv_count} rsp={x.scan_rsp_count} "
            f"uuids={uuids_total} mfg_companies={mfg_total}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
