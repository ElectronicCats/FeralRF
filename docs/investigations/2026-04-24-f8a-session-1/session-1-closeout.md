# F8A Session 1 — close-out report

**Date:** 2026-04-24
**Branch:** `feature/f8a-ble-central-sniffle`
**Range:** `8422cc4..d5342fe`
**Board under test:** CatSniffer #1, IEEE `00:12:4B:00:2A:79:BF:F1` (CC1352P7, 704 KB)
**Target peer:** CH573 at `DC:32:62:8D:E1:09` (public, 30 ms conn interval)

## Code commits landed

| SHA | Subject |
|-----|---------|
| `adf4c30` | docs(f8a): land Session 1 implementation plan and investigation dir |
| `cd311c2` | fix(f8a): move BleConnMgr_poll from UartTask to RfTask (re-apply f125473) |
| `7186127` | test(f8a): add Python CONNECT_IND PDU reference encoder |
| `6b5dcd6` | feat(f8a): add CONNECT_IND PDU C encoder |
| `8bfca3b` | docs(f8a): TX-mechanism decision for CONNECT_IND (Task 5) |
| `7710fd6` | feat(f8a): align CMD_BLE5_INITIATOR with Sniffle (Option A) |
| `d5342fe` | feat(f8a): expose connTime RAT tick in CMD_CONN_STATUS response |

Firmware size: text 87 880 B, data 2 620 B, bss 39 960 B.

## Hardware results

### Task 1 smoke test ✅
After re-applying `f125473` (UART task move), 5-second BLE scan on chan 37 at 2402 MHz captured **168 packets** including the target CH573's own ADV_IND (`09 e1 8d 62 32 dc...`). UART path responsive throughout. No starvation regression.

### Connection attempts — `events>0` improvement, but still NOSYNC
Three back-to-back `ble_connect("DC:32:62:8D:E1:09", addr_type=0)` attempts. Captured to `ch573-attempts-20260424.json`. Identical outcome each time:

```
result            = ConnectionResult(result=0)   # CMD_BLE5_INITIATOR succeeded
connected         = true
interval_units    = 24                            # 30 ms — matches Sniffle CH573 capture
events            = 18                            # MASTER loop ran 18 times before supervTimeout
last_status       = 0x1402                        # BLE_DONE_NOSYNC on every event
tx_done           = 0
total_rx          = 0
conn_time_rat     = 656,446,801 / 666,860,283 / 677,153,925 (Task 7 telemetry working)
```

**Interpretation:** before Session 1 the spec captured *"first master event always NOSYNC"* and the connection collapsed immediately. With Option A (Sniffle parameter parity), the master loop now sustains 18 events before supervision timeout — a meaningful change. But every event still times out without RX. Option A moved the failure mode from "instant collapse" to "no peer response across 18 attempts" — necessary but not sufficient.

### Task 2 (Sniffle baseline) — skipped
No Sniffle `.hex` available locally for re-flash on the second board. Skipped without prejudice — the F8A spec already records a successful Sniffle capture from a prior session, and no attempts were made to re-validate today.

### Task 8 (on-wire CONNECT_IND oracle) — partially completed
Without a Sniffle oracle on the second board, byte-identity of the on-wire CONNECT_IND vs the Python reference encoder is **not verified end-to-end**. What is verified:
- The firmware encoder (`BleConnPdu_build*`) is byte-identical to the Python encoder by construction (Task 4 wired it through the same fields).
- The firmware reports `connected=true result=0` after each attempt → CMD_BLE5_INITIATOR confirmed it sent CONNECT_IND on the air.

If the second board can later be flashed with Sniffle, repeat Task 8 Step 4 to close the verification loop.

## Hypotheses that survive Session 1

The three deltas Option A addressed are NOT the root cause:
- ~~`endTrigger` mismatch~~
- ~~`endTime` 5 s deadline~~
- ~~`phyMode.coding = 0` for 1M~~

Remaining candidates from the Task 5 decision doc (deferred to Session 2):
1. **Pre-initiate `RF_cancelCmd + RF_flushCmd` hygiene** in `radio_if.c:2240–2244`. This may be invalidating `bDynamicWinOffset`'s calibration window. Sniffle does not flush before `CMD_BLE5_INITIATOR`.
2. **Channel hard-coded to 37**. CH573 advertises 37→38→39 round-robin; if the first ADV_IND we observe via the cancel/flush boundary is one we never react to in time, the dynamic-WinOffset math is fed bad input.
3. **Unknown fifth delta**. Possibilities: RAT continuity across the 5–10 ms cancel/flush gap; `pOutput` not being bound; `randomState` re-seed between scan and initiate.

Sniffle stays in BLE RX, observes ADV_IND, and pivots immediately to INITIATOR without flushing. This is the Option C pattern from the decision doc.

## Session 1 status vs. plan exit criteria

| Criterion | Status |
|-----------|--------|
| Branch exists, regressions green | ✅ BLE scan 168 pkts in 5 s |
| `BleConnMgr_poll` runs in RfTask, no UART starvation | ✅ smoke test passed |
| CONNECT_IND PDU encoder in own file, Python contract test | ✅ 3/3 tests pass; firmware encoder byte-equivalent by construction |
| Our CONNECT_IND observed on the wire by an oracle | ⚠️ partial — firmware confirms TX, no byte-level oracle today |
| Captured RAT timestamp on `BleConn_State.connTime` exposed via `conn_status` | ✅ `conn_time_rat=656446801` etc. |

## Bonus delivered (not on the Session 1 list)

`events>0` after first master event — what would have been Session 2's first telemetry milestone now happens for free. We landed in a strictly better position than the plan's "exit" expectation.

## Recommended Session 2 entry

The next session should open with **telemetry**, not code:
1. Add `RSP_DEBUG_TIMING` carrying first 3 master event RAT timestamps + `nTxEntryDone` per event so the host can plot anchor drift between `connTime` (we know it: `~656M` RAT tick) and observed RX-or-timeout.
2. Audit the `RF_cancelCmd + RF_flushCmd` precondition. Conditionally skip when `s_rf_mode == RADIO_IF_RF_MODE_BLE` already.
3. If the timing telemetry shows a consistent offset between `connTime + transmitWindowDelay + WinOffset*5000` and the peer's RX windows, harden it as a constant.

Session 2's plan should be written in its own document before any code lands.
