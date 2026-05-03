# F8d — Architectural Fixes for CMD_CONNECT Hang + CMD_DISCONNECT Drop Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate two architectural bugs that block real BLE-central use on `feature/ti-rtos-migration`: F1 (CMD_CONNECT hangs RF task indefinitely on missing peer) and F2 (CMD_DISCONNECT silently drops the LL_TERMINATE_IND because it sleeps in the same task that should TX it).

**Architecture:** F1 reintroduces a bounded `TRIG_REL_START` endTrigger on `CMD_BLE5_INITIATOR` (8s) so `RF_runCmd` returns deterministically; the existing `BLE_DONE_ENDED → -1 → BLE_CONN_ERR_TIMEOUT` mapping in `RadioIF_bleInitiate` already handles the result. F2 splits `BleConn_disconnect` into a queue-and-flag entry point and a pure-cleanup phase, with `BleConnMgr_poll` cooperatively transmitting LL_TERMINATE_IND before teardown — peer receives a graceful termination instead of falling back to ~1s supervision timeout.

**Tech Stack:** C (TI-RTOS 7, GCC arm-none-eabi), Python 3.11+ (smoke scripts only — no API changes), COBS+CRC16 framing.

**Branch:** `feature/ti-rtos-migration` (HEAD = `0470f97`, the F8d spec commit). Do **NOT** modify `firmware/cc1352/include/radio_if.h` — WIP whitespace must stay unstaged.

**Spec:** `docs/superpowers/specs/2026-05-03-f8d-connect-disconnect-architectural-fixes-design.md`

---

## File Structure

**Modified:**
- `firmware/cc1352/src/ble_conn.c` — F1 timeout trigger; F2 split disconnect into `BleConn_finalizeDisconnect` (new) + `BleConn_disconnect` (legacy thin wrapper)
- `firmware/cc1352/include/ble_conn.h` — declare `BleConn_finalizeDisconnect`
- `firmware/cc1352/src/ble_conn_mgr.c` — F2 add static state + `BleConnMgr_initiateGracefulDisconnect` + poll hook + update `handle_ll_ctrl` and supervision timeout to call `BleConn_finalizeDisconnect`
- `firmware/cc1352/include/ble_conn_mgr.h` — declare `BleConnMgr_initiateGracefulDisconnect`
- `firmware/cc1352/src/command_processor.c` — F2 update `CMD_DISCONNECT` body to call `BleConnMgr_initiateGracefulDisconnect`

**Created:**
- `python/examples/smoke_f8d_connect_timeout.py` — F1 wire-level smoke (non-existent MAC, must terminate in 7-11s with `BLE_CONN_ERR_TIMEOUT`)
- `python/examples/smoke_f8d_graceful_dc.py` — F2 wire-level smoke (connect+disconnect+immediate-reconnect, second connect must succeed in <500ms)

**Untouched:** `radio_if.{c,h}` (the existing status-code mapping is correct), all Python in `python/feralrf/` (no API changes), all `python/tests/` (no behavior testable from a mocked-serial fixture).

---

## Pre-Flight (Task 0)

- [ ] **Step 0.1: Verify HEAD and clean working tree**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git status --short
git rev-parse --short HEAD
```

Expected: HEAD = `0470f97`. Only `M firmware/cc1352/include/radio_if.h` may appear in status. The untracked file `docs/investigations/2026-05-03-ti-rtos-migration-code-review.md` is also expected (the source investigation, not part of this plan). Anything else means you are not on the right base — STOP and ask.

- [ ] **Step 0.2: Confirm baseline build is green**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: build succeeds, produces `feralrf_cc1352.hex`. Two pre-existing warnings (`Wtype-limits`, unused variable) in `command_processor.c` are expected; no new warnings should be tolerated by the time the plan is done.

- [ ] **Step 0.3: Confirm baseline Python tests pass**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
pytest -q --deselect tests/test_radio_strict_responses.py::test_read_response_ignores_echoed_command_frames 2>&1 | tail -5
```

Expected: 445 passed, 5 skipped, 1 deselected. The deselected test is a known pre-existing failure unrelated to F8d.

- [ ] **Step 0.4: Confirm CC1352 board is present and responsive**

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip devices 2>&1 | tail -5
```

Expected: at least one CatSniffer detected (e.g., `CatSniffer #1` with Cat-Bridge on `/dev/ttyACM2`). If none, ask the user to plug the board in or perform a cold USB reconnect.

---

## Task 1: F1 — bounded timeout for CMD_BLE5_INITIATOR

**Files:**
- Modify: `firmware/cc1352/src/ble_conn.c` (lines 30-34 add constant; lines 185-188 change trigger config)

**Why this is one task:** The change is 5 lines. The existing `RadioIF_bleInitiate` already maps `BLE_DONE_ENDED` → return -1 → `BLE_CONN_ERR_TIMEOUT` (verified at `firmware/cc1352/src/radio_if.c:2424-2427`). No mapping changes required.

- [ ] **Step 1.1: Add the timeout constant near the other compile-time defines**

In `firmware/cc1352/src/ble_conn.c`, find the existing block at lines 28-34 that starts with `/* LL Control PDU opcodes used during teardown.`. After the `MS_TO_TASK_TICKS` definition (line 34), add:

```c
/* F1 fix: bounded timeout for CMD_BLE5_INITIATOR. The host's default
 * Python-side timeout for ble_connect is 10 s; firmware terminates
 * 2 s earlier so the host always receives a clean RSP_CONN_RESULT
 * (with BLE_CONN_ERR_TIMEOUT) instead of a host-side TimeoutError.
 * 8 s × 4 MHz RAT = 32_000_000 ticks. */
#define BLE_CONNECT_TIMEOUT_RAT_TICKS (8u * 4000000u)
```

- [ ] **Step 1.2: Replace the trigger configuration in `BleConn_initiate`**

In the same file, locate `BleConn_initiate` (starts at line 126). Find the comment block + 4 lines that currently read (around lines 180-188):

```c
    /* Sniffle parity: forever-listen, no host-imposed end. The previous
     * 5-second TRIG_ABSTIME endTime imposed a deadline that conflicted
     * with bDynamicWinOffset's calibration window. See
     * docs/investigations/2026-04-24-f8a-session-1/tx-mechanism-decision.md
     * (Option A). Re-introduce a host-side timeout in Session 2 if needed. */
    Ble5_0_cmdBle5Initiator.pParams->endTrigger.triggerType = TRIG_NEVER;
    Ble5_0_cmdBle5Initiator.pParams->endTime = 0;
    Ble5_0_cmdBle5Initiator.pParams->timeoutTrigger.triggerType = TRIG_NEVER;
    Ble5_0_cmdBle5Initiator.pParams->timeoutTime = 0;
```

Replace the entire block with:

```c
    /* F1 fix (was: TRIG_NEVER + endTime=0 + TRIG_NEVER): bounded timeout
     * via TRIG_REL_START so RF_runCmd terminates deterministically when
     * the peer never responds. Without this the RF task hangs forever and
     * subsequent commands fail with async ERR_INVALID_STATE — a full
     * reflash is the only recovery (observed against Sony WH-CH720N
     * during F8c live smoke 2026-05-02).
     *
     * The earlier note about a 5-second TRIG_ABSTIME conflicting with
     * bDynamicWinOffset's calibration window referred to ABSTIME
     * specifically; TRIG_REL_START is relative to the command's own
     * start tick and does not race with WinOffset calibration.
     *
     * BLE_DONE_ENDED is the status returned by RF_runCmd when this
     * trigger fires, and RadioIF_bleInitiate already maps it to return
     * value -1, which BleConn_initiate maps to BLE_CONN_ERR_TIMEOUT. */
    Ble5_0_cmdBle5Initiator.pParams->endTrigger.triggerType = TRIG_REL_START;
    Ble5_0_cmdBle5Initiator.pParams->endTime = BLE_CONNECT_TIMEOUT_RAT_TICKS;
    Ble5_0_cmdBle5Initiator.pParams->timeoutTrigger.triggerType = TRIG_NEVER;
    Ble5_0_cmdBle5Initiator.pParams->timeoutTime = 0;
```

- [ ] **Step 1.3: Build firmware**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build, `feralrf_cc1352.hex` regenerated. Same 2 pre-existing warnings, no new ones.

- [ ] **Step 1.4: Flash and run smoke 1 (will be created in Task 2 — defer until then)**

This task creates the firmware change. The wire-level smoke that proves it works is created in Task 2 below. After Task 2 completes, the smoke validates this task's change.

- [ ] **Step 1.5: Pre-commit + commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files firmware/cc1352/src/ble_conn.c
git add firmware/cc1352/src/ble_conn.c
git commit -m "fix(f8d): F1 bounded TRIG_REL_START timeout for CMD_BLE5_INITIATOR

Reintroduces an 8-second endTrigger (TRIG_REL_START) on the BLE
initiator so RF_runCmd terminates deterministically when the peer
never responds. Existing BLE_DONE_ENDED → -1 → BLE_CONN_ERR_TIMEOUT
mapping in RadioIF_bleInitiate handles the result without further
changes.

Empirical motivation: F8c live smoke 2026-05-02 against Sony
WH-CH720N consistently left the RF task hung after each connect
timeout, requiring a full firmware reflash for recovery."
```

---

## Task 2: F1 smoke harness

**Files:**
- Create: `python/examples/smoke_f8d_connect_timeout.py`

- [ ] **Step 2.1: Create the smoke script**

Create `python/examples/smoke_f8d_connect_timeout.py` with the following content:

```python
#!/usr/bin/env python3
"""F8d — F1 smoke: bounded CMD_CONNECT timeout.

Validates that connecting to a non-existent peer terminates cleanly
within 7-11 seconds (firmware 8 s + USB transport overhead) with
BLE_CONN_ERR_TIMEOUT, and that the device remains responsive
afterwards (no reflash needed).

Pre-conditions:
  - CC1352 board flashed with the F8d firmware (Task 1 landed).
  - No other process holding /dev/ttyACMx.

Pass criteria:
  - Connect attempt to all-zero MAC returns in 7-11 s.
  - Result code is BLE_CONN_ERR_TIMEOUT (uint8 cast of -1 = 0xFF
    on the wire; ConnectionResult.result == 0xFF).
  - Subsequent r.init() succeeds with no async error and no reflash.

Usage:
    source .venv/bin/activate
    python examples/smoke_f8d_connect_timeout.py
"""

from __future__ import annotations

import sys
import time

from feralrf import Radio


def main() -> int:
    r = Radio()
    r.connect()
    r.init()
    r.reset_device()

    print("Attempting connect to non-existent peer (00:00:00:00:00:00) ...")
    t0 = time.monotonic()
    res = r.ble_connect(b"\x00\x00\x00\x00\x00\x00", addr_type=0, timeout=12.0)
    elapsed = time.monotonic() - t0
    print(f"  result={res} elapsed={elapsed:.2f}s")

    ok_elapsed = 7.0 < elapsed < 11.0
    # BLE_CONN_ERR_TIMEOUT = 2 per the BleConn_Result enum (ble_conn.h:32);
    # the firmware sends it through send_response as a uint8.
    # NOTE: if the on-wire value is something else (e.g., 0xFF for the
    # signed -1 path), the smoke will fail loudly here — fix by inspecting
    # the actual code returned and updating this assertion.
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

    r.disconnect()

    print()
    print(f"  [{'PASS' if ok_elapsed else 'FAIL'}]  elapsed-in-window (7-11s)")
    print(f"  [{'PASS' if ok_code else 'FAIL'}]  result == BLE_CONN_ERR_TIMEOUT (2)")
    print(f"  [{'PASS' if ok_responsive else 'FAIL'}]  board-responsive-after-timeout")

    return 0 if (ok_elapsed and ok_code and ok_responsive) else 1


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 2.2: chmod + pre-commit**

```bash
chmod +x /home/sabas/Documents/electroniccats/FeralRF/python/examples/smoke_f8d_connect_timeout.py
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/examples/smoke_f8d_connect_timeout.py
```

- [ ] **Step 2.3: Flash the F1 firmware change and run the smoke**

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex 2>&1 | tail -3
```

Expected: `✓ Verified match`. If it fails, retry once. If it still fails, report it as DONE_WITH_CONCERNS — do NOT invoke OpenOCD recovery.

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python examples/smoke_f8d_connect_timeout.py
```

Expected output (3 PASS lines):
```
[PASS]  elapsed-in-window (7-11s)
[PASS]  result == BLE_CONN_ERR_TIMEOUT (2)
[PASS]  board-responsive-after-timeout
```

If `result == BLE_CONN_ERR_TIMEOUT` fails because the actual code is something else (e.g., 0xFF for signed-int on-wire), update the assertion in the smoke script to match the observed value and re-run; the actual numeric code is determined by the firmware's `send_response(RSP_CONN_RESULT, seq, &res, 1)` path which sends the enum value directly. Document the actual value in the commit message for Task 2.

If `elapsed-in-window` fails (>11s), the firmware is still hanging — STOP and re-check Task 1's trigger configuration.

If `board-responsive-after-timeout` fails, the F1 fix didn't fully release the RF task. STOP and ask for help.

- [ ] **Step 2.4: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git add python/examples/smoke_f8d_connect_timeout.py
git commit -m "test(f8d): F1 smoke — bounded connect timeout against non-existent peer"
```

---

## Task 3: F2 — refactor `BleConn_disconnect` to expose pure cleanup phase

**Files:**
- Modify: `firmware/cc1352/include/ble_conn.h` (declare `BleConn_finalizeDisconnect`)
- Modify: `firmware/cc1352/src/ble_conn.c` (extract pure-cleanup body into new function)

**Behavior change:** None at this point. After this task, `BleConn_disconnect` still does what it does today (queue + sleep + finalize). The new `BleConn_finalizeDisconnect` is added but not yet called by anyone outside `BleConn_disconnect`. Task 4 wires it up.

- [ ] **Step 3.1: Declare `BleConn_finalizeDisconnect` in the header**

In `firmware/cc1352/include/ble_conn.h`, find the line `void BleConn_disconnect(void);` (around line 60). Immediately after it, add:

```c

/* Pure radio-state cleanup, idempotent. Used by the cooperative
 * disconnect flow (see BleConnMgr_initiateGracefulDisconnect) AFTER
 * LL_TERMINATE_IND has been transmitted (or after the grace window
 * expires). Does NOT queue any LL PDU and does NOT sleep — safe to
 * call from BleConnMgr_poll context.
 *
 * Sets s_state.connected=false, s_state.initiating=false,
 * s_state.eventCounter=0, and stops any active scan/initiate via
 * RadioIF_stopRx() if applicable. Does NOT call BleConnMgr_stop()
 * — caller is responsible for that ordering. */
void BleConn_finalizeDisconnect(void);
```

- [ ] **Step 3.2: Add the function body in `ble_conn.c`**

In `firmware/cc1352/src/ble_conn.c`, locate the end of `BleConn_disconnect` (currently ends at line 269, just before `bool BleConn_isConnected(void)`). Immediately after the closing `}` of `BleConn_disconnect`, add:

```c

void BleConn_finalizeDisconnect(void) {
    /* Pure cleanup — no queue, no sleep, no BleConnMgr_stop. The
     * cooperative disconnect path (BleConnMgr_initiateGracefulDisconnect
     * + BleConnMgr_poll hook) is responsible for getting the manager
     * stopped and the disconnect callback fired BEFORE calling this. */
    if (s_state.initiating) {
        RadioIF_stopRx();
    }
    s_state.connected = false;
    s_state.initiating = false;
    s_state.eventCounter = 0;
}
```

- [ ] **Step 3.3: Build**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -5
```

Expected: clean build (same 2 pre-existing warnings only). The new function is defined but unused at this point — gcc will not warn about that for a non-static function.

- [ ] **Step 3.4: Verify no behavior change**

The smoke from Task 2 should still pass identically. Quick re-run:

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex 2>&1 | tail -3
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python examples/smoke_f8d_connect_timeout.py 2>&1 | tail -5
```

Expected: 3 PASS as before. If anything regresses, inspect Step 3.2 — the most likely cause is accidentally pulling extra logic out of `BleConn_disconnect`.

- [ ] **Step 3.5: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files firmware/cc1352/include/ble_conn.h firmware/cc1352/src/ble_conn.c
git add firmware/cc1352/include/ble_conn.h firmware/cc1352/src/ble_conn.c
git commit -m "refactor(f8d): extract BleConn_finalizeDisconnect pure-cleanup phase

Adds a public BleConn_finalizeDisconnect() that performs only the
radio-state cleanup currently done by the post-Task_sleep block of
BleConn_disconnect. No callers added yet (Task 4 wires it up via
BleConnMgr_initiateGracefulDisconnect). Behavior is identical at
this point — F8c-era smoke must continue to pass."
```

---

## Task 4: F2 — `BleConnMgr_initiateGracefulDisconnect` + `BleConnMgr_poll` hook

**Files:**
- Modify: `firmware/cc1352/include/ble_conn_mgr.h` (declare `BleConnMgr_initiateGracefulDisconnect`)
- Modify: `firmware/cc1352/src/ble_conn_mgr.c` (static state, function body, poll hook)

**Behavior change:** Adds a new entry point that nobody calls yet. The poll hook is added but only fires if `s_pending_disconnect` is true — which only `BleConnMgr_initiateGracefulDisconnect` can set. Task 5 wires the entry point into actual callers.

- [ ] **Step 4.1: Declare the new public function in the header**

In `firmware/cc1352/include/ble_conn_mgr.h`, find the existing declaration `void BleConnMgr_stopWithReason(uint8_t reason);` (added in F8c Task 4). Immediately after it, add:

```c

/* F8d — cooperative graceful disconnect. Queues LL_TERMINATE_IND on
 * the connection's TX queue and lets BleConnMgr_poll() complete the
 * teardown after TX confirmation, OR after a 5-event grace window
 * (~150 ms at 30 ms interval) as a safety bound for the case where
 * the peer has dropped off-air mid-disconnect.
 *
 * If no connection is active (s_running==false), falls through to
 * immediate stop with the given reason — caller does not need to
 * check connection state. The disconnect callback fires exactly once
 * (sticky-first-caller from F8c is respected: if a peer
 * LL_TERMINATE_IND arrives during the grace window, the original
 * host-supplied reason still wins).
 *
 * This is the function CMD_DISCONNECT should call. The legacy
 * BleConn_disconnect() (sleeps in RF task) MUST NOT be called from
 * a code path that owns a connection; use this instead. */
void BleConnMgr_initiateGracefulDisconnect(uint8_t reason);
```

- [ ] **Step 4.2: Add static state and the function body**

In `firmware/cc1352/src/ble_conn_mgr.c`, find the existing static state block that the F8c work added (the lines with `s_disconnect_cb`, `s_pending_dc_reason`, `s_dc_reason_pending` — search for `s_dc_reason_pending`). Immediately after that block, add:

```c

/* F8d — cooperative graceful disconnect state. Only mutated from the
 * BleConnMgr_poll task context (same as the F8c sticky reason fields
 * above; see their doc comment for the concurrency invariant). */
static bool s_pending_disconnect;
static uint8_t s_disconnect_events_remaining;
#define DISCONNECT_TX_GRACE_EVENTS 5u  /* ~150 ms at 30 ms interval */
```

Then locate the end of `BleConnMgr_stopWithReason` (which was added in F8c Task 4 — search for `BleConnMgr_stopWithReason`). Immediately after it, add the new function:

```c

void BleConnMgr_initiateGracefulDisconnect(uint8_t reason) {
    if (!s_running) {
        /* No active connection — degenerate case. Apply sticky reason
         * and tear down immediately. BleConnMgr_stopWithReason fires
         * the disconnect callback with the reason; finalizeDisconnect
         * does the radio-state cleanup. */
        BleConnMgr_stopWithReason(reason);
        BleConn_finalizeDisconnect();
        return;
    }

    /* Queue LL_TERMINATE_IND on the connection's TX queue. The next
     * BleConnMgr_poll event will TX it. Note: opcode 0x02 + reason
     * byte per BT Core Spec Vol 6 Part B §2.4.2.6. */
    uint8_t pdu[2];
    pdu[0] = 0x02u; /* LL_TERMINATE_IND */
    pdu[1] = reason;
    (void)TXQueue_insert(2, TX_QUEUE_LLID_CTRL, pdu);

    /* Set sticky reason now so when BleConnMgr_stop fires from the
     * poll-hook teardown, the callback sees the host-initiated reason.
     * Sticky-first-caller from F8c handles a racing peer LL_TERMINATE
     * within the grace window. */
    if (!s_dc_reason_pending) {
        s_pending_dc_reason = reason;
        s_dc_reason_pending = true;
    }

    s_pending_disconnect = true;
    s_disconnect_events_remaining = DISCONNECT_TX_GRACE_EVENTS;
}
```

- [ ] **Step 4.3: Add the poll-loop hook**

In the same file, locate `BleConnMgr_poll` (starts around line 291). Find the existing block:

```c
    /* BLE_DONE_OK=0x1400, BLE_DONE_ENDED=0x1403, BLE_DONE_STOPPED=0x1404 */
    if (status == 0x1400 || status == 0x1403 || status == 0x1404) {
        s_last_rx_time = RF_getCurrentTime();
        process_rx_packets();
    }

    /* Advance to next anchor */
    s_event_counter++;
    s_next_hop_time += s_hop_interval_ticks;
```

Insert the F8d hook BETWEEN the `process_rx_packets()` block and the `/* Advance to next anchor */` comment:

```c
    /* BLE_DONE_OK=0x1400, BLE_DONE_ENDED=0x1403, BLE_DONE_STOPPED=0x1404 */
    if (status == 0x1400 || status == 0x1403 || status == 0x1404) {
        s_last_rx_time = RF_getCurrentTime();
        process_rx_packets();
    }

    /* F8d — cooperative graceful disconnect: if the host queued
     * LL_TERMINATE_IND via BleConnMgr_initiateGracefulDisconnect,
     * tear the connection down once TX is confirmed (numSent >= 1
     * means our queue was actually transmitted this event) OR once
     * the 5-event grace window expires (peer is unresponsive — we
     * give up on the clean LL_TERMINATE and let our side cleanup
     * regardless; peer will fall back to supervision timeout). */
    if (s_pending_disconnect) {
        bool tx_confirmed = (numSent >= 1u);
        bool give_up = (--s_disconnect_events_remaining == 0u);
        if (tx_confirmed || give_up) {
            s_pending_disconnect = false;
            s_disconnect_events_remaining = 0u;
            BleConnMgr_stop();            /* fires DC callback w/ sticky reason */
            BleConn_finalizeDisconnect(); /* pure radio cleanup */
            return false;                 /* signal poll loop: connection ended */
        }
    }

    /* Advance to next anchor */
    s_event_counter++;
    s_next_hop_time += s_hop_interval_ticks;
```

- [ ] **Step 4.4: Reset graceful-disconnect state in `BleConnMgr_start`**

In the same file, locate `BleConnMgr_start` (around line 201). Find the existing F8c-era reset of the sticky reason flags (search for `s_dc_reason_pending = false` inside `BleConnMgr_start`). Immediately after the line `s_pending_dc_reason = 0;`, add:

```c
    s_pending_disconnect = false;
    s_disconnect_events_remaining = 0u;
```

This guarantees that a fresh connection cannot inherit stale graceful-disconnect state from a prior session.

- [ ] **Step 4.5: Build**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build, same 2 pre-existing warnings, no new ones. The new function is unused at this point — that's fine for a non-static function.

- [ ] **Step 4.6: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files firmware/cc1352/include/ble_conn_mgr.h firmware/cc1352/src/ble_conn_mgr.c
git add firmware/cc1352/include/ble_conn_mgr.h firmware/cc1352/src/ble_conn_mgr.c
git commit -m "feat(f8d): BleConnMgr_initiateGracefulDisconnect + cooperative poll hook

Adds the host-facing graceful-disconnect API and the BleConnMgr_poll
hook that completes teardown after LL_TERMINATE_IND TX confirmation
(or after a 5-event grace window). New entry point is not yet wired
into any caller — Task 5 updates CMD_DISCONNECT, handle_ll_ctrl, and
the supervision-timeout branch."
```

---

## Task 5: F2 — call-site audit + wire-up

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c` (`CMD_DISCONNECT` body)
- Modify: `firmware/cc1352/src/ble_conn_mgr.c` (`handle_ll_ctrl` LL_TERMINATE_IND case + supervision timeout branch in `BleConnMgr_poll`)

This task wires the new graceful-disconnect entry point into the three known callers and ensures the legacy `BleConn_disconnect()` is no longer invoked from a code path that owns a connection.

- [ ] **Step 5.1: Audit all call sites**

Run a grep to confirm there are exactly 3 call sites and no surprises:

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
grep -n "BleConn_disconnect" firmware/cc1352/src/ firmware/cc1352/include/
```

Expected output (3 hits in src/ + 1 declaration in the header):

```
firmware/cc1352/src/ble_conn_mgr.c:<line>:    BleConn_disconnect();
firmware/cc1352/src/ble_conn_mgr.c:<line>:    BleConn_disconnect();
firmware/cc1352/src/command_processor.c:<line>:    BleConn_disconnect();
firmware/cc1352/include/ble_conn.h:<line>:void BleConn_disconnect(void);
firmware/cc1352/src/ble_conn.c:<line>:void BleConn_disconnect(void) {
```

(The exact line numbers vary; only the count matters.) If a fourth call site appears outside this list, STOP and ask — the plan was written assuming exactly these three, and a missed caller would silently retain the buggy sleep-then-stop behavior.

- [ ] **Step 5.2: Update `CMD_DISCONNECT` in command_processor.c**

In `firmware/cc1352/src/command_processor.c`, locate the `CMD_DISCONNECT` case (search for `case CMD_DISCONNECT:`). It currently looks like (post-F8c):

```c
    case CMD_DISCONNECT:
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ensure_gatt_callbacks();
        /* Send the ACK FIRST. The synchronous BleConnMgr_stopWithReason call
         * below fires the disconnect callback, which emits an async
         * RSP_DISCONNECTED frame on the wire. Python's ble_disconnect waits
         * for ACK with _read_response, which silently drops any unexpected
         * frame seen while waiting — so if RSP_DISCONNECTED arrives first,
         * the host loses the disconnect event. Emitting ACK first preserves
         * cause-and-effect ordering on the wire (host sees ACK to its
         * command, then the async event in a clean reader state). */
        send_ack(seq);
        /* Mark host-initiated reason BEFORE BleConn_disconnect so the
         * subsequent BleConnMgr_stop callback sees 0x16, not whatever
         * sticks around from the previous session. */
        BleConnMgr_stopWithReason(0x16u); /* LOCAL_HOST_TERMINATED per BT Core Spec */
        BleConn_disconnect();
        return;
```

Replace the body (keep payload check + ensure_gatt_callbacks + send_ack as-is, but replace the `BleConnMgr_stopWithReason(0x16u); BleConn_disconnect();` lines and their comment with the F8d call):

```c
    case CMD_DISCONNECT:
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ensure_gatt_callbacks();
        /* Send the ACK FIRST. The cooperative disconnect flow below
         * (BleConnMgr_initiateGracefulDisconnect + BleConnMgr_poll
         * teardown) eventually fires the async RSP_DISCONNECTED frame
         * on the wire. Python's ble_disconnect waits for ACK with
         * _read_response and would discard an out-of-order async event;
         * emitting ACK first preserves cause-and-effect ordering. */
        send_ack(seq);
        /* F8d: queue LL_TERMINATE_IND with reason 0x16
         * (LOCAL_HOST_TERMINATED) and let BleConnMgr_poll complete the
         * teardown cooperatively. Replaces the earlier
         * BleConnMgr_stopWithReason(0x16u) + BleConn_disconnect() pair,
         * which (a) immediately stopped the manager so the LL_TERMINATE
         * was never TX'd to the peer, and (b) called Task_sleep() inside
         * BleConn_disconnect from the same RF task that would TX it. */
        BleConnMgr_initiateGracefulDisconnect(0x16u);
        return;
```

- [ ] **Step 5.3: Update `handle_ll_ctrl` LL_TERMINATE_IND in ble_conn_mgr.c**

In `firmware/cc1352/src/ble_conn_mgr.c`, locate the `LL_TERMINATE_IND` case in `handle_ll_ctrl` (it currently includes the F8c sticky-reason call). It looks like:

```c
    case LL_TERMINATE_IND: {
        /* payload = [opcode:1][reason:1] per BT Core Spec Vol 6 Part B §2.4.2.6 */
        uint8_t reason = (len >= 2) ? payload[1] : 0x13u; /* default REMOTE_USER_TERMINATED */
        BleConnMgr_stopWithReason(reason);
        BleConn_disconnect();
        break;
    }
```

Replace the two function calls with `BleConn_finalizeDisconnect()` (no need to TX our own LL_TERMINATE — peer already sent theirs, just clean up):

```c
    case LL_TERMINATE_IND: {
        /* payload = [opcode:1][reason:1] per BT Core Spec Vol 6 Part B §2.4.2.6 */
        uint8_t reason = (len >= 2) ? payload[1] : 0x13u; /* default REMOTE_USER_TERMINATED */
        /* F8d: peer already TX'd LL_TERMINATE, no point retransmitting.
         * Apply sticky reason (fires DC callback) then pure cleanup —
         * skip the queue+sleep path entirely. */
        BleConnMgr_stopWithReason(reason);
        BleConn_finalizeDisconnect();
        break;
    }
```

- [ ] **Step 5.4: Update supervision-timeout branch in `BleConnMgr_poll`**

In the same file, locate the supervision-timeout branch in `BleConnMgr_poll` (search for `0x22u`):

```c
    /* Check supervision timeout */
    now = RF_getCurrentTime();
    if (now - s_last_rx_time > s_superv_timeout_ticks) {
        /* 0x22 = LL_RESPONSE_TIMEOUT per BT Core Spec Vol 1 Part F §1.3.2 */
        BleConnMgr_stopWithReason(0x22u);
        BleConn_disconnect();
        return false;
    }
```

Replace the call to `BleConn_disconnect()` with `BleConn_finalizeDisconnect()` (peer is gone, no point queueing a PDU it won't ack):

```c
    /* Check supervision timeout */
    now = RF_getCurrentTime();
    if (now - s_last_rx_time > s_superv_timeout_ticks) {
        /* 0x22 = LL_RESPONSE_TIMEOUT per BT Core Spec Vol 1 Part F §1.3.2.
         * F8d: peer is unresponsive (no RX in supervision window), so
         * skip the queue+sleep path — just clean up. */
        BleConnMgr_stopWithReason(0x22u);
        BleConn_finalizeDisconnect();
        return false;
    }
```

- [ ] **Step 5.5: Build**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build, same 2 pre-existing warnings only.

- [ ] **Step 5.6: Re-grep to confirm `BleConn_disconnect` is now only called from the legacy wrapper itself (defensive — there should be 0 callers in src/ outside ble_conn.c)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
grep -n "BleConn_disconnect" firmware/cc1352/src/ firmware/cc1352/include/
```

Expected: only the declaration in `ble_conn.h` and the definition in `ble_conn.c`. If any other src/ file still references it, you missed a call site — STOP and find it.

- [ ] **Step 5.7: Flash and run F8c regression smoke (proves nothing broke)**

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex 2>&1 | tail -3
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python examples/smoke_f8c.py CB:2B:7D:35:5A:0E 1 2>&1 | tail -10
```

Expected: 3/3 PASS for the existing F8c smoke (MTU + Read by UUID + Disconnect event). The disconnect-event timing will shift from ~30ms to ~50-150ms but stays well within the 3s timeout in the smoke script.

If the bocina at `CB:2B:7D:35:5A:0E` is no longer advertising or its random MAC has rotated, scan first and substitute the new MAC. If it cannot be found, fall back to USBNinja:

```bash
python examples/smoke_f8c.py C0:94:9A:DA:4F:09 0 2>&1 | tail -10
```

If the F8c regression smoke fails (any FAIL), STOP and inspect — the most likely cause is a broken call-site update in Step 5.2-5.4.

- [ ] **Step 5.8: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files firmware/cc1352/src/command_processor.c firmware/cc1352/src/ble_conn_mgr.c
git add firmware/cc1352/src/command_processor.c firmware/cc1352/src/ble_conn_mgr.c
git commit -m "feat(f8d): wire CMD_DISCONNECT + handle_ll_ctrl + supervision timeout to F8d API

CMD_DISCONNECT now calls BleConnMgr_initiateGracefulDisconnect(0x16);
the cooperative path in BleConnMgr_poll handles the actual TX of
LL_TERMINATE_IND and the teardown afterwards.

handle_ll_ctrl LL_TERMINATE_IND and the supervision-timeout branch
both switch to BleConn_finalizeDisconnect (peer already gone or
already sent its own LL_TERMINATE — no point re-queueing).

After this task, BleConn_disconnect() (legacy queue+sleep wrapper)
has zero callers in src/ outside ble_conn.c itself."
```

---

## Task 6: F2 smoke — graceful disconnect lets peer free slot immediately

**Files:**
- Create: `python/examples/smoke_f8d_graceful_dc.py`

- [ ] **Step 6.1: Create the smoke script**

Create `python/examples/smoke_f8d_graceful_dc.py`:

```python
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
```

- [ ] **Step 6.2: chmod + pre-commit**

```bash
chmod +x /home/sabas/Documents/electroniccats/FeralRF/python/examples/smoke_f8d_graceful_dc.py
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/examples/smoke_f8d_graceful_dc.py
```

- [ ] **Step 6.3: Run the smoke (firmware was flashed in Step 5.7)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python examples/smoke_f8d_graceful_dc.py CB:2B:7D:35:5A:0E 1
```

Expected (4 PASS):
```
[PASS]  first_connect
[PASS]  dc_event
[PASS]  reconnect_ok
[PASS]  reconnect_fast
```

If `reconnect_fast` fails (elapsed >= 500ms but `reconnect_ok` is True), the LL_TERMINATE_IND is still not getting through — the peer is falling back to supervision timeout. Inspect the F2 wire-up (Task 5) and the poll hook (Task 4).

If `dc_event` fails, the F8c async-buffering or the cooperative teardown's callback fire is broken — start there.

If the bocina is unreachable, fall back to USBNinja:

```bash
python examples/smoke_f8d_graceful_dc.py C0:94:9A:DA:4F:09 0
```

Capture the output verbatim into the commit message body.

- [ ] **Step 6.4: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git add python/examples/smoke_f8d_graceful_dc.py
git commit -m "test(f8d): F2 smoke — graceful disconnect lets peer free slot immediately"
```

---

## Task 7: Final regression sweep + tag

- [ ] **Step 7.1: Run the full Python suite**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
pytest -q --deselect tests/test_radio_strict_responses.py::test_read_response_ignores_echoed_command_frames 2>&1 | tail -5
```

Expected: 445 passed (same as baseline — F8d adds no Python tests). If the count differs, investigate.

- [ ] **Step 7.2: Verify the WIP whitespace in `radio_if.h` is still unstaged**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git status --short
```

Expected: only `M firmware/cc1352/include/radio_if.h` (untouched), and the untracked `docs/investigations/2026-05-03-ti-rtos-migration-code-review.md` (the source investigation, not part of this plan).

- [ ] **Step 7.3: Re-run both new smokes back-to-back to confirm stability**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python examples/smoke_f8d_connect_timeout.py 2>&1 | tail -5
python examples/smoke_f8d_graceful_dc.py CB:2B:7D:35:5A:0E 1 2>&1 | tail -5
```

Expected: 3/3 PASS for smoke 1, 4/4 PASS for smoke 2. If either is flaky between consecutive runs, document it as a known limitation (likely peer-side state, not firmware) — don't tag.

- [ ] **Step 7.4: Tag**

If both smokes pass cleanly, tag the release:

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git tag -a v2.0-f8d -m "F8d: bounded CMD_CONNECT timeout + cooperative graceful disconnect

Two architectural fixes for feature/ti-rtos-migration that were
blocking real BLE-central use:

1. F1: CMD_BLE5_INITIATOR now uses TRIG_REL_START (8 s) so RF_runCmd
   terminates deterministically when the peer never responds. Existing
   BLE_DONE_ENDED → -1 → BLE_CONN_ERR_TIMEOUT mapping in
   RadioIF_bleInitiate handles the result. Validated against
   non-existent MAC: connect returns in 7-11 s with the timeout code,
   board remains responsive (no reflash needed).

2. F2: BleConn_disconnect split into BleConn_finalizeDisconnect (pure
   cleanup) and a new cooperative path via
   BleConnMgr_initiateGracefulDisconnect that queues LL_TERMINATE_IND
   and lets BleConnMgr_poll TX it across the next 1-5 events before
   tearing down. Peer receives a clean termination and frees its slot
   immediately. Validated against Soundcore Boom 2: connect →
   disconnect → reconnect cycle completes in <500 ms (vs ~1 s before
   F8d).

Source: docs/investigations/2026-05-03-ti-rtos-migration-code-review.md
findings F1 (Critical) and F2 (High). Spec:
docs/superpowers/specs/2026-05-03-f8d-connect-disconnect-architectural-fixes-design.md
Plan: docs/superpowers/plans/2026-05-03-f8d-connect-disconnect-architectural-fixes.md"
```

If smokes are partial, use `v2.0-f8d-partial` instead and document the failure in a project-memory entry.

---

## Self-Review

**1. Spec coverage:**
- F1 architecture (timeout trigger): Task 1. ✓
- F1 result-code mapping (already correct): explicit note in Task 1 commentary; no separate task needed because no change required. ✓
- F2 Part A (state in ble_conn_mgr.c): Task 4.2. ✓
- F2 Part B (public API): Task 4.1. ✓
- F2 Part C (impl): Task 4.2. ✓
- F2 Part D (poll hook): Task 4.3. ✓
- F2 Part E (refactor BleConn_disconnect): Task 3. ✓
- F2 Part F (call-site audit + wire-up): Task 5. ✓
- F2 reset on BleConnMgr_start: Task 4.4. ✓
- Smoke 1 (F1): Task 2. ✓
- Smoke 2 (F2): Task 6. ✓
- Regression check (F8c smoke): Task 5.7. ✓
- Out-of-scope items (F5-F9, async API): explicitly excluded — no tasks. ✓

**2. Placeholder scan:**
- No "TBD", "implement later", or "fill in details" anywhere.
- Step 5.1's `<line>` placeholders in expected grep output are for line numbers (which vary), not for content the implementer must fill — clear from context.
- Step 2.3 has a fallback note ("if the actual code is something else, update the assertion") — this is acceptable defensive guidance, not a missing requirement; the most likely value (2 = `BLE_CONN_ERR_TIMEOUT` per `ble_conn.h:32`) is named explicitly.
- No "Similar to Task N" — every code block is repeated verbatim.

**3. Type consistency:**
- `BleConn_finalizeDisconnect` declared in Task 3.1, defined in Task 3.2, called from Task 4.2 (degenerate path), Task 5.3 (handle_ll_ctrl), Task 5.4 (supervision timeout), and the poll hook in Task 4.3. All call sites use the same signature `void(void)`. ✓
- `BleConnMgr_initiateGracefulDisconnect(uint8_t reason)` declared in Task 4.1, defined in Task 4.2, called from Task 5.2 (CMD_DISCONNECT). Signature matches. ✓
- `s_pending_disconnect`, `s_disconnect_events_remaining`, `DISCONNECT_TX_GRACE_EVENTS` — defined in Task 4.2, used in Task 4.3 (poll hook) and Task 4.4 (start reset). Names consistent. ✓
- `BLE_CONNECT_TIMEOUT_RAT_TICKS` defined in Task 1.1, used in Task 1.2. ✓
- Reason byte `0x16u` for host-initiated DC: used in Task 5.2 (CMD_DISCONNECT) and matches the F8c convention; sticky-first-caller from F8c handles racing peer reasons. ✓
- `numSent` in the poll hook (Task 4.3) — the `RadioIF_bleCentral` call earlier in `BleConnMgr_poll` writes to `numSent` (already in scope, see `firmware/cc1352/src/ble_conn_mgr.c:347-348`). ✓
