# F8A Session 2 — Fix NOSYNC at the master event

> Continues `feature/f8a-ble-central-sniffle`. Entry condition: Session 1 closed
> at commit `b3f365e`, with `events>0` confirmed but every event NOSYNC.

**Goal:** turn `last_status=0x1402` into `0x1400` (BLE_DONE_OK) with at least one
RX from the peer, on the same 30 ms CH573 connection that Sniffle handles.

**Hypothesis priority (cheapest experiment first):**

1. **H1 — `RF_cancelCmd + RF_flushCmd` invalidates `bDynamicWinOffset`.**
   Today `RadioIF_bleInitiate` cancels the live RX, flushes the data queue,
   recreates it, then posts the initiator. Sniffle does none of this — it lets
   the radio stay warm and calls `RF_runCmd(RF_cmdBle5Initiator)` directly.
   With `bDynamicWinOffset=1`, the initiator computes WinOffset from the time
   between an observed ADV_IND and the to-be-transmitted CONNECT_IND. If the
   cancel/flush gap (5–10 ms wallclock) breaks RAT continuity or tosses the
   adv timestamp the calibration relied on, the WinOffset goes on the wire
   stale and the peer's listening window misses our CONNECT_IND tail by enough
   that *every* subsequent master event is also off. Test: try with the
   cancel/flush removed. ~10 min experiment.

2. **H2 — initiator's `connectTime + 4000` seed is too short.**
   Sniffle uses the same `+ 4000` constant. Falsifiable: bump to `+ 40000`
   (10 ms) and see if it changes. ~5 min.

3. **H3 — channel-37 hard-code starves the dynamic offset of input.**
   CH573 advertises 37→38→39 round robin. If our cancel/flush happens to land
   us with no fresh ADV_IND on 37 within reach, dynamicWinOffset has nothing
   to calibrate against. ~30 min if H1 and H2 fail.

If H1/H2/H3 fail, fall back to telemetry (per-event debug response with
start/end RAT + status). That is heavier so we punt unless cheap experiments
exhaust.

**Out of scope:** GATT round-trip, ICall cleanup, regression matrix, tag
`v2.0-f8a`. Those move to Session 3 once a sustained connection is in hand.

---

## File structure

| File | Change |
|------|--------|
| `firmware/cc1352/src/radio_if.c` | Modify `RadioIF_bleInitiate()` lines 2227–2272 to skip the cancel/flush when already in BLE RX mode (H1). Possibly bump connectTime offset (H2). |
| `firmware/cc1352/src/ble_conn.c` | If H3 needed: parameterize the channel passed to `Ble5_0_cmdBle5Initiator.channel`. |
| `docs/investigations/2026-04-24-f8a-session-1/` | Append capture per experiment (rename dir to f8a-session-2 going forward). |

---

## Task 1 — H1: skip cancel/flush, retain warm RX state

- [ ] **Step 1** — Read `firmware/cc1352/src/radio_if.c:2227–2272` to confirm the
      current sequence is exactly: ensure-BLE-mode → cancel-RX-cmd → flush-all
      → recreate data queue → set `pRxQ` → set `connectTime/endTime` → reset
      status → `RF_runCmd`.

- [ ] **Step 2** — Edit so that when `s_rf_mode == RADIO_IF_RF_MODE_BLE` and
      `s_rf_rx_cmd >= 0`:
      - **Stop** calling `RF_cancelCmd` / `RF_flushCmd`.
      - **Keep** the existing data queue (do not recreate it).
      - **Still** clear `s_rx_running` and reset `s_rf_rx_cmd` so the host-side
        accounting agrees, but only after the initiator returns.

      The minimal diff: behind a bool gate, replace the cancel/flush/recreate
      block with a comment noting the new behavior. Keep the legacy path for
      the not-already-in-BLE case (cold initiate from another PHY).

- [ ] **Step 3** — Build clean (`cd firmware/cc1352/build && cmake --build .`).

- [ ] **Step 4** — Flash board #1 via catnip device 1. Run the same 3-attempt
      capture script as Session 1 close-out and append output to
      `docs/investigations/2026-04-24-f8a-session-1/ch573-h1-experiment-<ts>.json`.

- [ ] **Step 5** — Decide:
      - If `last_status` becomes `0x1400` (or anything not `0x1402`), commit
        the change and open Task 4 (sustained-connection check).
      - If still `0x1402` on every event, revert this change locally
        (`git checkout -- radio_if.c`) and proceed to Task 2.

## Task 2 — H2: bump `connectTime + 4000` to `+ 40000`

Only run if Task 1 didn't change `last_status`.

- [ ] **Step 1** — Edit `firmware/cc1352/src/radio_if.c` (the line that sets
      `Ble5_0_cmdBle5Initiator.pParams->connectTime = now + 4000u`). Try
      `+ 40000u` (10 ms). 4 MHz RAT clock.
- [ ] **Step 2** — Build, flash, capture. Same JSON pattern.
- [ ] **Step 3** — Decide as in Task 1 Step 5.

## Task 3 — H3: cycle channel 37/38/39

Only if H1 and H2 fail.

- [ ] **Step 1** — Make `Ble5_0_cmdBle5Initiator.channel` a parameter set from
      a host-supplied byte (default channel kept at 37 for backward compat).
      Add a `channel` arg to `BleConn_initiate` and to the protocol command
      payload. **OR**: simpler — try chans 37, 38, 39 in sequence in firmware
      with a 100 ms gap each, until one yields events>0 + non-NOSYNC.
- [ ] **Step 2** — Build, flash, capture.
- [ ] **Step 3** — Decide.

## Task 4 — Sustained-connection check

Run on whichever Task above succeeded.

- [ ] **Step 1** — `ble_connect`, then `conn_status` every 1 s for 10 s. Expect
      `events` to grow without `connected` flipping false.
- [ ] **Step 2** — If stable, capture transcript and commit. If not, characterize
      and decide whether to escalate to telemetry.

## Telemetry escape hatch (Task 5 — only if H1/H2/H3 all fail)

Add `RSP_DEBUG_TIMING (0xA8)` carrying for the most recent master event:
`startTime` (4 B), `endTime` (4 B), `status` (2 B), `numSent` (1 B), repeated
for the last 5 events as a 55-byte buffer. Wire host parse + dump.

This is heavier so we only build it if the cheap experiments don't move
`last_status`.

---

## Closing

When `last_status=0x1400` is observed and at least one packet RX'd from the
peer, commit, capture, and write a Session 2 close-out at
`docs/investigations/2026-04-24-f8a-session-1/session-2-closeout.md`. Tag/Session
3 are deferred to Session 3.
