# F8A Session 5 — close-out report

**Date:** 2026-04-27
**Branch:** `feature/f8a-ble-central-sniffle`
**Range from Session 4:** `68e4736..5a41580`
**Outcome:** ⚠️ **F8A NOSYNC bug fixed, exit criterion not yet fully met.**
The connection now sustains 100+ events at `last_status=0x1400 (BLE_DONE_OK)`
against CH573, but `gatt_discover()` times out — separate ATT/RX issue,
not the master-anchor bug F8A was filed to chase. `v2.0-f8a` deferred
until GATT round-trip closes against a real peripheral.

## What happened

The Session 4 closeout listed three candidate parameter mismatches
(hopIncrement / accessAddr-CRC / first-event off-by-one). Wire-vs-state
diagnostics this session ruled out all three — the on-air CONNECT_IND
matched our `s_state` and `s_ll_data` byte for byte. The actual bug was
**not in the candidate list**: it was a double-application of the BLE
spec's `transmitWindowDelay + WinOffset*1.25 ms` correction on top of
TI's already-corrected `connectTime`.

## Commits

| SHA       | Subject |
|-----------|---------|
| `5a41580` | feat(f8a): add CMD_DEBUG_CONN_PARAMS + post-initiator s_state re-snapshot |
| `ea9bf3f` | fix(f8a): match Sniffle's anchor formula — close BLE central NOSYNC |

## Diagnostic chain

1. **Re-snapshot AA / CRC / hopIncrement / channelMap from `s_ll_data`
   post-initiator** (mirrors the existing `winOffset` re-snapshot).
   Hypothesis: SDK overwrites these like it does WinOffset. Build, flash,
   retest → still NOSYNC. So the SDK does *not* rewrite them; our
   `s_state` already had correct values. **Hypothesis falsified.**

2. **Add `CMD_DEBUG_CONN_PARAMS` (0x48 / 0xA9, 50-byte response)** that
   dumps `s_state.{accessAddr, crcInit, hopIncrement, channelMap,
   winOffset, eventCounter, connTime, useCsa2, ...}` plus the raw
   22-byte `s_ll_data` buffer. Capture paired with Sniffle pcap on
   board #2 listening on adv ch 37. Result: **wire AA / CRC / WinSize /
   WinOffset / Interval / Timeout / ChM / HopIncrement match
   `s_state` and `s_ll_data` exactly.** `ChSel=0` confirmed on the wire
   (Session 4's worry that the chSel=0 commit hadn't taken effect was
   wrong — the old `session-3-sniffle.pcap` had been captured before
   that commit landed). All three Session 4 candidates falsified.

3. **Try removing `rf_patch_mce_bt5`** (Sniffle uses only the CPE patch
   for BLE 1M; we ship CPE + MCE). Hypothesis: extra MCE patch breaks
   1M packet encoding. Result: still NOSYNC. **Falsified, MCE patch
   restored.**

4. **Diff `BleConnMgr_start`'s anchor formula against
   `Sniffle/fw/RadioTask.c:467`.** Sniffle uses
   `nextHopTime = connTime - AO_TARG + hopInterval`. We were using
   `connTime + transmitWindowDelay + WinOffset*5000 - AO_TARG +
   hopInterval`. The extra `transmitWindowDelay + WinOffset*5000`
   (5000 RAT ticks = 1.25 ms; with WinOffset=21 in the test run that's
   105 000 ticks = 26.25 ms) put our anchor ~27 ms past the slave's
   listening window. Drop the extra terms → match Sniffle → **fix.**

## Result

Default 30 ms / 1 s connection against CH573 with a 3-second linger:

| Metric       | Pre-fix (`68e4736`)       | Post-fix (`ea9bf3f`)         |
|--------------|---------------------------|------------------------------|
| connected    | False                     | **True**                     |
| events       | 0                         | **101**                      |
| last_status  | `0x1402` (BLE_DONE_NOSYNC) | **`0x1400` (BLE_DONE_OK)**   |
| tx_done      | 0                         | **101**                      |
| nTx / event  | 1 (auto-empty only)       | **2** (queued + auto-empty)  |
| numSent / event | 0 (no ACKs)            | **1** (every queued PDU ACK'd) |
| pktStatus    | `0x00`                    | **`0x48`** (bLastEmpty + bLastAck) |

Master and slave are exchanging LL packets cleanly for the entire
linger period. The link is sustained; F8A's "first master event NOSYNC"
saga is closed.

## What's still broken — `gatt_discover()` hangs

After `connected=True events=18`, calling `r.gatt_discover()` times out
waiting for `RSP_GATT_SERVICE`. Symptoms:

- `pktStatus` shows `bLastEmpty=1, bLastAck=1` every event — slave is
  ACKing our master's TX with empty PDUs.
- `nRxOk = 0` (no good-CRC non-empty payloads decoded).
- `total_rx = 0` reported in `conn_status`.

So the slave DOES respond on the wire (we'd otherwise NOSYNC), but its
non-empty ATT responses are not reaching the host RX queue. Three
candidate causes for the next session:

1. **`bAutoFlushEmpty=0`** keeps empty PDUs in the RX queue but might
   shadow non-empty packets we expect to see (queue full, dropped).
   Try `bAutoFlushEmpty=1` to keep the queue clean for non-empty data.
2. **`AttClient_poll()` may not be queueing the ATT request** — the
   `TXQueue_insert(0, TX_QUEUE_LLID_DATA_CONT, NULL)` after it inserts
   an empty PDU that may overwrite the ATT request entry.
3. **`RadioIF_processBlePackets()` may discard non-empty PDUs** during
   queue drain because the LLID classification routes them somewhere
   they're being silently dropped. Audit `radio_if.c::processBlePackets`
   and `tx_queue.c` LLID handling.

Cheapest first experiment: dump the RF data queue contents directly
after the first `events=5..10` window via a new debug command
(`CMD_DEBUG_RX_QUEUE`?), and confirm whether non-empty PDUs ever
arrive at the queue level. If they do but never reach the host, the
bug is in `processBlePackets`. If they don't, the bug is upstream
(slave never sends them, or our master's own RX config drops them).

## Permanent infrastructure landed

- **`CMD_DEBUG_CONN_PARAMS` (0x48) / `RSP_DEBUG_CONN_PARAMS` (0xA9).**
  50-byte snapshot of post-initiator `s_state` + raw `s_ll_data[22]`.
  Use `r.debug_conn_params().ll_data_decoded()` to compare against on-air
  pcaps. Designed to spot any future SDK-rewrite mismatches.
- **`BleConn_initiate` re-snapshot of AA / CRC / hopIncrement /
  channelMap from `s_ll_data` after the initiator returns.** Today
  they're identical to the build_ll_data values, but the pattern keeps
  s_state authoritative against any future SDK behaviour change in this
  area.
- **`BleConn_getLlData()` getter** — read-only view of the LLData buffer
  for diagnostics.

## Why `v2.0-f8a` is still deferred

F8A's exit criterion in
`docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md` §5 reads:
"`demo_ble_connect_gatt.py … connects, discovery completes, read OK,
disconnect clean`." We meet `connects` and `disconnect clean` but not
`discovery completes` / `read OK`. Tagging now would be dishonest.

## Open items into Session 6

1. **Debug GATT path:** ATT request queueing or RX-queue drain are the
   two most likely culprits.
2. **Regression matrix:** validate BLE 1M / 2M / Coded S2 / Coded S8 +
   IEEE 802.15.4 + Sub-1GHz still work (anchor change only affects the
   central path, but worth confirming).
3. **Async RF err 0x2F** still showing on first `init()` after reset —
   carry-over Session 3 cleanup item.
4. **Once GATT works:** tag `v2.0-f8a`, write a final summary, and
   unblock F8.
