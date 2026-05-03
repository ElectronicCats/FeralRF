#!/usr/bin/env python3
"""F8d — F1 smoke: bounded CMD_CONNECT timeout.

Validates that connecting to a non-existent peer terminates cleanly
within 7.5-9.0 seconds (firmware 8 s + USB transport overhead) with
BLE_CONN_ERR_TIMEOUT, and that the device remains responsive
afterwards (no reflash needed).

Pre-conditions:
  - CC1352 board flashed with the F8d firmware (Task 1 landed).
  - No other process holding /dev/ttyACMx.

Pass criteria:
  - Connect attempt to all-zero MAC returns in 7.5-9.0 s.
  - Result code is BLE_CONN_ERR_TIMEOUT, which is the uint8 enum
    value 2 per BleConn_Result in ble_conn.h:32 (the firmware sends
    the enum value directly, NOT a signed cast).
  - Subsequent r.init() succeeds with no async error and no reflash.

Usage:
    source .venv/bin/activate
    python examples/smoke_f8d_connect_timeout.py
"""

from __future__ import annotations

import time

from feralrf import Radio


def main() -> int:
    r = Radio()
    r.connect()
    r.init()
    r.reset_device()

    try:
        print("Attempting connect to non-existent peer (00:00:00:00:00:00) ...")
        # Use 00:00:00:00:00:00 as the target MAC. This is technically the
        # null/broadcast address, but the firmware does not short-circuit
        # it — verified empirically that 8.00s elapsed proves the request
        # reached RF_runCmd and waited the full firmware timeout path.
        t0 = time.monotonic()
        res = r.ble_connect(b"\x00\x00\x00\x00\x00\x00", addr_type=0, timeout=12.0)
        elapsed = time.monotonic() - t0
        print(f"  result={res} elapsed={elapsed:.2f}s")

        ok_elapsed = 7.5 < elapsed < 9.0
        # BLE_CONN_ERR_TIMEOUT = 2 per BleConn_Result enum (ble_conn.h:32).
        # Firmware sends the enum value directly via send_response(RSP_CONN_RESULT,
        # seq, &res, 1). Validated empirically against this exact build.
        ok_code = res.result == 2
        print(f"  elapsed-in-window: {ok_elapsed}    correct-timeout-code: {ok_code}")

        print("Verifying board still responsive (init must succeed) ...")
        try:
            info = r.init()
            print(f"  init OK: {info}")
            ok_responsive = True
        except Exception as e:
            print(f"  init FAILED: {e}")
            ok_responsive = False
    finally:
        r.disconnect()

    print()
    print(f"  [{'PASS' if ok_elapsed else 'FAIL'}]  elapsed-in-window (7.5-9.0s)")
    print(f"  [{'PASS' if ok_code else 'FAIL'}]  result == BLE_CONN_ERR_TIMEOUT (2)")
    print(f"  [{'PASS' if ok_responsive else 'FAIL'}]  board-responsive-after-timeout")

    return 0 if (ok_elapsed and ok_code and ok_responsive) else 1


if __name__ == "__main__":
    raise SystemExit(main())
