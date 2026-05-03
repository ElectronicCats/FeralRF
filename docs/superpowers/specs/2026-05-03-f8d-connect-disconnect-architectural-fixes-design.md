# F8d — Architectural Fixes for CMD_CONNECT Hang and CMD_DISCONNECT Drop

**Branch:** `feature/ti-rtos-migration`
**Date:** 2026-05-03
**Predecessors:** v2.0-f8c (HEAD `5d4138d`)
**Investigation source:** `docs/investigations/2026-05-03-ti-rtos-migration-code-review.md` findings F1 (Critical) and F2 (High)

---

## Goal

Eliminate the two architectural bugs in the BLE central path of `feature/ti-rtos-migration`:

- **F1 (Critical):** `CMD_CONNECT` can hang the RF task indefinitely. The current `CMD_BLE5_INITIATOR` configuration disables both `endTrigger` and `timeoutTrigger` (`TRIG_NEVER`) and is launched via blocking `RF_runCmd()`. If the target peer never appears, the host's Python timeout fires but the firmware-side RF task remains blocked and cannot process recovery commands. Observed empirically during F8c live smoke against Sony WH-CH720N — every connect attempt left the board in a state where subsequent `init()` calls returned async `ERR_INVALID_STATE` (0x05) and required a full reflash to recover.

- **F2 (High):** `CMD_DISCONNECT` silently drops the `LL_TERMINATE_IND` PDU. `BleConn_disconnect()` queues the PDU and then calls `Task_sleep()` for three connection intervals (~90ms) from the same RF task that owns `BleConnMgr_poll()`. Because the only task that can transmit the queued PDU is asleep, the PDU is never sent, then `BleConnMgr_stop()` runs and discards the queue. Peer falls back to supervision timeout (~1s) instead of receiving a graceful termination — flaky for back-to-back reconnect flows.

Both fixes use the **minimal-disruption** approach (chosen as scope option "A" during brainstorming): no Python API changes, no async-API refactor. Firmware-side timeout for F1, cooperative state machine in `BleConnMgr_poll` for F2.

---

## Architecture

### F1: bounded RF timeout for CMD_BLE5_INITIATOR

In `firmware/cc1352/src/ble_conn.c::BleConn_initiate` (around lines 180-188), replace the `endTrigger` configuration of the BLE5_INITIATOR command:

```c
/* Before — causes indefinite RF task hang on missing peer: */
s_initiate_cmd.endTrigger.triggerType = TRIG_NEVER;
s_initiate_cmd.timeoutTrigger.triggerType = TRIG_NEVER;

/* After — bounded timeout, deterministic exit path: */
s_initiate_cmd.endTrigger.triggerType = TRIG_REL_START;
s_initiate_cmd.endTime = BLE_CONNECT_TIMEOUT_RAT_TICKS;
s_initiate_cmd.timeoutTrigger.triggerType = TRIG_NEVER;
```

A new `#define BLE_CONNECT_TIMEOUT_RAT_TICKS (8u * 4000000u) /* 8 s at 4 MHz RAT */` is added near the top of `ble_conn.c`. The 8-second value is chosen so the firmware terminates the operation comfortably before the host's default Python timeout (10s on `Radio.ble_connect`), so the host always sees a clean `RSP_CONN_RESULT` with the timeout code instead of a host-side `TimeoutError`.

`TRIG_REL_START` is preferred over `timeoutTrigger` because it terminates the radio operation determinatively at deadline regardless of internal state (scanning, syncing, connecting). This is the TI-recommended pattern for "stop after N RAT ticks no matter what".

The existing `BleConn_initiate` already maps `result == -1` to `BLE_CONN_ERR_TIMEOUT`. The TI status code returned by `RF_runCmd` when the trigger fires is one of `BLE_DONE_ENDED` (0x1403), `BLE_DONE_RXTIMEOUT` (0x1407), or `BLE_DONE_NOSYNC` (0x1402). The implementation must map all three to `BLE_CONN_ERR_TIMEOUT` so the host always receives a consistent code.

### F2: cooperative graceful disconnect

The fix splits the work between three modules. The key insight: the only safe place to TX a control PDU is from inside `BleConnMgr_poll()` between connection events. So `CMD_DISCONNECT` must NOT directly tear down — it must only signal intent and let `BleConnMgr_poll` complete the work over the next 1-5 events.

**Part A — new state in `firmware/cc1352/src/ble_conn_mgr.c`:**

```c
static bool s_pending_disconnect;
static uint8_t s_disconnect_events_remaining;
#define DISCONNECT_TX_GRACE_EVENTS 5u  /* ~150 ms at 30 ms interval */
```

**Part B — new public API in `firmware/cc1352/include/ble_conn_mgr.h`:**

```c
/* Initiate a graceful disconnect. Queues LL_TERMINATE_IND and lets
 * BleConnMgr_poll() complete the teardown after TX confirmation (or
 * after DISCONNECT_TX_GRACE_EVENTS events as a safety bound).
 *
 * If no connection is active, falls through to immediate stop with the
 * given reason — caller does not need to check connection state.
 *
 * The disconnect callback (BleConnMgr_setDisconnectCb) fires exactly
 * once with `reason` once teardown completes (sticky-first-caller from
 * F8c is respected; if a peer LL_TERMINATE_IND arrives in the grace
 * window, the original host reason still wins). */
void BleConnMgr_initiateGracefulDisconnect(uint8_t reason);
```

**Part C — implementation in `ble_conn_mgr.c`:**

```c
void BleConnMgr_initiateGracefulDisconnect(uint8_t reason) {
    if (!s_running) {
        /* No active connection — degenerate case. Apply sticky reason
         * and tear down immediately. */
        BleConnMgr_stopWithReason(reason);
        BleConn_finalizeDisconnect();
        return;
    }
    /* Queue LL_TERMINATE_IND on the connection's TX queue. The next
     * BleConnMgr_poll() event will TX it. */
    uint8_t pdu[2] = {LL_TERMINATE_IND, reason};
    TXQueue_insert(2, TX_QUEUE_LLID_CTRL, pdu);
    /* Set sticky reason now so when BleConnMgr_stop fires later the
     * callback sees the host-initiated reason (matches F8c semantics). */
    if (!s_dc_reason_pending) {
        s_pending_dc_reason = reason;
        s_dc_reason_pending = true;
    }
    s_pending_disconnect = true;
    s_disconnect_events_remaining = DISCONNECT_TX_GRACE_EVENTS;
}
```

**Part D — hook in `BleConnMgr_poll`:**

After the existing event-processing block (after `RadioIF_bleCentral` returns and `process_rx_packets` runs), insert:

```c
if (s_pending_disconnect) {
    bool tx_confirmed = (numSent >= 1u);
    bool give_up = (--s_disconnect_events_remaining == 0u);
    if (tx_confirmed || give_up) {
        s_pending_disconnect = false;
        s_disconnect_events_remaining = 0u;
        BleConnMgr_stop();              /* fires DC callback w/ sticky reason */
        BleConn_finalizeDisconnect();   /* pure radio cleanup */
        return false;                   /* signal poll loop: done */
    }
}
```

**Part E — refactor `firmware/cc1352/src/ble_conn.c`:**

Split `BleConn_disconnect` into a clear two-phase API:

- `BleConn_finalizeDisconnect(void)` (NEW): pure radio-state cleanup. Sets `s_state.connected=false`, `s_state.initiating=false`, `s_state.eventCounter=0`, calls `RadioIF_stopRx()` if `initiating` was true. No queue, no sleep, no `BleConnMgr_stop`. Idempotent.

- `BleConn_disconnect(void)` (KEPT, refactored): becomes a thin wrapper that calls `BleConnMgr_initiateGracefulDisconnect(0x13u)` (default REMOTE_USER_TERMINATED reason) for backward compatibility with callers that don't have a specific reason. Internal callers should prefer the explicit-reason API.

**Part F — call-site audit**

After Parts A-E land, all three known call sites of `BleConn_disconnect` must be reviewed:

1. `command_processor.c::CMD_DISCONNECT` — replace `BleConnMgr_stopWithReason(0x16u); BleConn_disconnect();` with single call `BleConnMgr_initiateGracefulDisconnect(0x16u);`. The F8c-era `send_ack(seq)` placement at the top of the case stays.

2. `ble_conn_mgr.c::handle_ll_ctrl LL_TERMINATE_IND` — peer already sent its own LL_TERMINATE_IND, so we should NOT retransmit. Replace `BleConnMgr_stopWithReason(reason); BleConn_disconnect();` with `BleConnMgr_stopWithReason(reason); BleConn_finalizeDisconnect();`. Direct cleanup, no queue.

3. `ble_conn_mgr.c::supervision timeout branch in BleConnMgr_poll` — peer is gone (no RX in supervision window), no point queueing a PDU it won't ack. Replace `BleConnMgr_stopWithReason(0x22u); BleConn_disconnect();` with `BleConnMgr_stopWithReason(0x22u); BleConn_finalizeDisconnect();`.

A `grep -rn "BleConn_disconnect" firmware/cc1352/` is mandatory before the implementation commit lands to confirm no fourth caller exists.

---

## Components / Data Flow

**Disconnect flow after F8d (host-initiated path):**

```
host: r.ble_disconnect()
  → CMD_DISCONNECT seq=N over USB serial
firmware command_processor: handle CMD_DISCONNECT
  → send_ack(N)
  → BleConnMgr_initiateGracefulDisconnect(0x16)
    → TXQueue_insert(LL_TERMINATE_IND, reason=0x16)
    → s_pending_disconnect=true, s_disconnect_events_remaining=5
  → return
firmware BleConnMgr_poll: next event tick (≤30 ms)
  → RadioIF_bleCentral builds queue, transmits LL_TERMINATE_IND
  → numSent >= 1 → tx_confirmed=true
  → s_pending_disconnect=false
  → BleConnMgr_stop() fires disconnect callback w/ reason 0x16
    → gatt_on_disconnected(0x16)
      → OutputIF_sendResponse(RSP_DISCONNECTED, seq=0, [0x16])
  → BleConn_finalizeDisconnect() clears radio state
  → return false (poll loop ends iteration)
host: ble_disconnect returns on RSP_ACK (seq=N)
host: read_disconnect_events() yields DisconnectEvent(reason=0x16)
host: peer can immediately accept a new ble_connect (no supervision wait)
```

**Worst-case path (peer disappeared mid-disconnect):**

After 5 events without TX confirmation (~150ms), `give_up` triggers. Same teardown runs, `RSP_DISCONNECTED(0x16)` still emitted, host sees clean event. Peer falls back to supervision timeout on its side (host doesn't care).

**Connect-timeout flow after F1:**

```
host: r.ble_connect(addr, addr_type, timeout=10.0)
  → CMD_CONNECT seq=N
firmware: BleConn_initiate sets up CMD_BLE5_INITIATOR with TRIG_REL_START + 8s
  → RF_runCmd(...) blocks RF task
  ... peer never appears ...
  → 8s elapsed, RF subsystem returns (status: BLE_DONE_ENDED or similar)
firmware: BleConn_initiate maps to BLE_CONN_ERR_TIMEOUT (-1)
  → RSP_CONN_RESULT seq=N payload=[0xFF] (uint8 cast of -1)
host: ble_connect returns ConnectionResult(result=-1, is_ok=False)
host: subsequent commands work normally — no reflash needed
```

---

## Error Handling

- **F1: unmapped TI status code.** If `RF_runCmd` returns a status code not currently mapped to `BLE_CONN_ERR_TIMEOUT` (e.g., `BLE_DONE_ENDED` 0x1403 was previously only an internal value), the host receives `BLE_CONN_ERR_RF` (-3) instead of `BLE_CONN_ERR_TIMEOUT` (-1). Functional outcome same (connect failed), but error code is misleading. The plan includes explicit mapping of all trigger-end-related status codes (`BLE_DONE_ENDED`, `BLE_DONE_RXTIMEOUT`, `BLE_DONE_NOSYNC`) to `BLE_CONN_ERR_TIMEOUT`.

- **F2: peer LL_TERMINATE during grace window.** Both host CMD_DISCONNECT and peer LL_TERMINATE_IND can race within 150ms. Per the F8c sticky-first-caller rule, whoever set `s_dc_reason_pending` first wins. If host went first, callback fires with `0x16` even though peer also sent its own reason. Acceptable: host initiated, host's reason is "the truth" of the intent.

- **F2: TX queue full when initiateGracefulDisconnect runs.** Defensive — if `TXQueue_insert` fails (queue full), we still set the flag and let the poll loop drain naturally. The PDU is lost but the disconnect still completes after `give_up` fires. Worst case is the same as A2 (truly minimal) for this single edge case.

- **F2: `s_pending_disconnect` set but `s_running` becomes false externally.** Cannot happen in current code — the only path that flips `s_running=false` is `BleConnMgr_stop`, which is only called from `BleConnMgr_initiateGracefulDisconnect`'s own teardown branch and from `BleConn_finalizeDisconnect` (which doesn't touch `s_running`). The F8d code paths preserve this invariant.

---

## Testing

**Unit tests (Python):** None. F8d does not change wire format, Python API, or any behavior testable from a mocked-serial fixture. The existing 445 tests must continue to pass (regression check).

**Smoke 1 — `python/examples/smoke_f8d_connect_timeout.py`:**

Verifies F1. Connect attempt to a non-existent MAC must terminate cleanly within ~10s (firmware 8s + transport overhead) with `BLE_CONN_ERR_TIMEOUT (-1)`, and the device must remain responsive afterwards (no reflash needed).

```python
import time
from feralrf import Radio
r = Radio(); r.connect(); r.init(); r.reset_device()
print("Attempting connect to non-existent peer...")
t0 = time.monotonic()
res = r.ble_connect(b"\x00\x00\x00\x00\x00\x00", addr_type=0, timeout=12.0)
elapsed = time.monotonic() - t0
print(f"  result={res} elapsed={elapsed:.2f}s")
assert 7.0 < elapsed < 11.0, f"Expected 7-11s timeout, got {elapsed}s"
assert res.result == 0xFF, f"Expected BLE_CONN_ERR_TIMEOUT (-1 = 0xFF), got {res.result}"
# Verify board still responsive
info = r.init()
print(f"Post-timeout init: OK ({info})")
print("[PASS] F1 connect timeout deterministic")
```

**Smoke 2 — `python/examples/smoke_f8d_graceful_dc.py`:**

Verifies F2. Connect to a cooperative peer, disconnect, immediately reconnect — second connect must succeed in <500ms (proves peer received LL_TERMINATE_IND and freed its slot, vs falling back to ~1s supervision timeout).

Default target: Soundcore Boom 2 at `CB:2B:7D:35:5A:0E` (per F8c live-smoke records). Override via argv.

```python
import sys, time
from feralrf import Radio

mac = sys.argv[1] if len(sys.argv) > 1 else "CB:2B:7D:35:5A:0E"
addr_type = int(sys.argv[2]) if len(sys.argv) > 2 else 1

addr_le = bytes(int(p, 16) for p in reversed(mac.split(":")))
r = Radio(); r.connect(); r.init(); r.reset_device()

print(f"Cycle 1: connect to {mac}")
res = r.ble_connect(addr_le, addr_type=addr_type, timeout=10.0)
assert res.is_ok, f"first connect failed: {res}"
time.sleep(0.3)

print("Disconnect (graceful)")
r.ble_disconnect(timeout=3.0)
ev = next(iter(r.read_disconnect_events(timeout=3.0)), None)
assert ev is not None and ev.reason == 0x16, f"DC event missing or wrong: {ev}"

print("Cycle 2: immediate reconnect (must succeed in <500ms)")
t0 = time.monotonic()
res2 = r.ble_connect(addr_le, addr_type=addr_type, timeout=2.0)
elapsed = time.monotonic() - t0
print(f"  result={res2} elapsed={elapsed:.2f}s")
assert res2.is_ok, f"reconnect failed: {res2}"
assert elapsed < 0.5, f"Expected <500ms reconnect (proof of LL_TERMINATE delivery), got {elapsed}s"
r.ble_disconnect(timeout=3.0)
print("[PASS] F2 graceful disconnect lets peer free slot immediately")
```

**Regression smoke:** `python/examples/smoke_f8c.py CB:2B:7D:35:5A:0E 1` must continue to print `3/3 PASS`. Disconnect event arrival timing will shift from ~30ms to ~50-150ms, well within the 3s timeout window in the script.

**No firmware-side debug telemetry by default.** If the cooperative path proves flaky in smoke 2, add `s_dc_tx_confirmed_count` / `s_dc_grace_expired_count` debug counters and expose via `CMD_DEBUG_TIMING` as a follow-up.

---

## Risks + Rollback

1. **F1 status code mapping (low):** Mitigated by the plan's task that explicitly maps `BLE_DONE_ENDED` (0x1403), `BLE_DONE_RXTIMEOUT` (0x1407), and `BLE_DONE_NOSYNC` (0x1402) all to `BLE_CONN_ERR_TIMEOUT`. Smoke 1 catches a miss in ~5 minutes.

2. **F2 call-site leak (medium):** Mitigated by an explicit audit task — the implementation plan requires `grep -rn "BleConn_disconnect" firmware/cc1352/` and per-site review before the final commit. Three known sites today: `command_processor.c::CMD_DISCONNECT`, `ble_conn_mgr.c::handle_ll_ctrl LL_TERMINATE_IND`, `ble_conn_mgr.c::supervision timeout branch`.

3. **F2 race during grace window (low):** Sticky-first-caller from F8c handles it correctly. Documented in the Error Handling section above.

4. **Rollback strategy:**
   - F1 fix is one isolated commit (~5 lines in `ble_conn.c`). Independent revert leaves F2 intact.
   - F2 fix is two commits (refactor + wire-up). Revert the wire-up commit to restore the F8c behavior; the refactor (split into `BleConn_finalizeDisconnect`) is harmless on its own.
   - No Python rollback needed (no Python changes).

---

## Out of Scope

Findings F5, F6, F7, F8, F9 from `docs/investigations/2026-05-03-ti-rtos-migration-code-review.md` are explicitly excluded. They are bounded-scope housekeeping items and earn their own follow-up plan (F8e or similar). They do not block real BLE central use; F1 and F2 do.

The full async-API refactor of `Radio.ble_connect` (chosen scope option "C" during brainstorming) is also explicitly excluded. F8d preserves the existing synchronous host API. If post-F8d use exposes overlap requirements (e.g., need to scan while connecting), that motivates a separate architectural plan.
