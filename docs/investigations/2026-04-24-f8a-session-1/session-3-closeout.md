# F8A Session 3 — close-out report

**Date:** 2026-04-27
**Branch:** `feature/f8a-ble-central-sniffle`
**Range from Session 2:** `fcb016f..HEAD`
**Outcome:** ⚠️ **F8A NOT CLOSED.** Telemetry plumbing landed and the timing
hypothesis was disproven, but NOSYNC persists for a deeper reason.
`v2.0-f8a` tag deferred until Session 4.

## What landed

| Commit | Subject |
|--------|---------|
| `f6304a5` | feat(f8a): add CMD_DEBUG_TIMING / RSP_DEBUG_TIMING (0x47/0xA8) |
| `1388c16` | telemetry(f8a): paired FeralRF/Sniffle capture + offset analyzer |
| (this) | wip(f8a): groundwork — CSA#1 fallback, anchor correction, Sniffle parity |

## Telemetry that worked

`RSP_DEBUG_TIMING` (0xA8) returns a 16-entry ring buffer of master-event
records: `(eventIdx u16, startRAT u32, endRAT u32, status u16, numSent u8)`.
Surface in Python via `Radio.debug_timing()`. With it we measured every
post-CONNECT_IND master event timestamp on the RAT clock. Conclusions
that survive Session 3:

- The firmware **is** running master events on a perfect 30 ms cadence
  starting at `connTime + correction`, where `correction = transmitWindowDelay
  + WinOffset * 1.25 ms` and `WinOffset` is read back from `s_ll_data[8..9]`
  after the SDK overwrites it (with `bDynamicWinOffset=1`).
- For every event `nTxEntryDone == 0`. Master never consumes a TX queue
  entry — even though its status is `BLE_DONE_NOSYNC` (0x1402, "no sync
  received from slave"), which the TI mailbox header claims requires a
  master TX to have happened first.
- Sniffle (board #2) sees `TRANSITION: DATA from STATIC` immediately after
  our CONNECT_IND and `TRANSITION: STATIC from DATA` ~1 s later — i.e.
  CH573 accepts the connection and stays in DATA state for the full
  supervision-timeout window, but no data-channel PDUs are observed in
  either direction during that window.

## What got falsified

| Hypothesis | Evidence |
|------------|----------|
| **H4 — anchor offset = transmitWindowDelay + WinOffset×1.25 ms** | Pcap+RAT correlation showed a clean 6250 µs gap with WinOffset=4 in the SDK-overridden CONNECT_IND. Adding that constant to `s_next_hop_time` did not change `BLE_DONE_NOSYNC`. |
| **H5 — `bAutoFlushEmpty=1` mismatch with Sniffle** | Flipping to `=0` (Sniffle parity) changed nothing. NOSYNC every event. |
| **H6 — CSA#2 vs CSA#1 mismatch (CH573 is BLE 4.2 → uses CSA#1)** | Forcing `useCsa2=false` and `chSel=0` in CONNECT_IND, plus implementing a CSA#1 hop in `BleConnMgr_poll`, did not change NOSYNC and did not produce any visible master TX in Sniffle's data-channel follow. |

## What survives Session 3 (groundwork in this branch)

- `CMD_DEBUG_TIMING` / `RSP_DEBUG_TIMING` and Python parser.
- Paired-capture script (`f8a_session3_capture.py`) and offset analyzer
  (`f8a_session3_offset_analysis.py`). The analyzer's pcap parser handles
  Sniffle's 14-byte BTLE pHdr and the µs/ns magic distinction.
- `s_state.winOffset` is now the SDK-actual WinOffset (read back from
  `s_ll_data[8..9]` post-initiate), not the random value the host provided.
- `BleConnMgr_start` applies a dynamic anchor correction
  `connTime + transmitWindowDelay + WinOffset*1.25 ms - AO_TARG + hopInterval`.
  Mathematically aligned with the BLE Core Spec peer first-listen formula.
- `BleConnMgr_poll` falls back to a CSA#1 legacy hop (`(N+1) * hopIncrement
  mod 37`) when `useCsa2=false`. Required for any BLE-<5.0 peer.
- Initiator now defaults `chSel=0` (CSA#1) for compatibility with BLE 4.x
  peers like CH573.
- `bAutoFlushEmpty` set to `0` for Sniffle parity (received empty PDUs are
  kept in the RX queue).

None of these change observable behavior against CH573 today, but each one
removes a confounder for Session 4.

## Remaining mystery — pursue first in Session 4

The signal we cannot explain is `nTxEntryDone == 0` combined with
`status == 0x1402`. Per TI's mailbox header (cc13xx/cc26xx SDK 8.30
`rf_ble_cmd.h`):

- `BLE_DONE_NOSYNC` = "Operation ended because no synchronization received
  from the slave **after the master had transmitted**".

So either (a) the RF core is exiting the master state machine before the
TX state, or (b) `nTxEntryDone` is reported only for queue entries that
were transmitted **and** acknowledged — in which case the master is
sending the implicit empty PDU on air but the queued entry is never
consumed because the connection never reaches the RX-data state.

Things to instrument first in Session 4:

1. **Observe the air directly for our master TX.** Today's Sniffle setup
   only follows the LL connection; if our CSA hop computation is even
   slightly off the actual master TX is invisible. Run Sniffle in
   `passive scanning, no follow` mode AND pin it to one specific data
   channel (e.g. ch 12 for hopIncrement=12) to check whether our master
   ever puts energy on the air at all.
2. **Add `RF_cmdBle5Master.status` and `pOutput.pktStatus` to telemetry.**
   We currently only expose the final mailbox status. Per-event
   `pktStatus.bTimeStampValid` and `nRxOk` would tell us whether the
   command is even running through the RX state.
3. **Compare against a Sniffle CENTRAL build, not just the sniffer.**
   Sniffle has a `central_role` test in `python_cli/sniffle/sniffle_hw.py`.
   If the same hardware can connect to CH573 as a Sniffle CENTRAL, the
   bug is in our SDK config; if Sniffle ALSO can't, the bug is in CH573
   or in board-level antenna/RF and our master is fine.
4. **Increase `connInterval` and `supervTimeout` to give wider listening
   windows.** Currently 30 ms / 1 s. Try 100 ms / 5 s — easier to land
   the first TX inside the peer's window.

## Why this session can't tag `v2.0-f8a`

`v2.0-f8a`'s exit criterion is "demo_ble_connect_gatt … connects, discovery
completes, read OK, disconnect clean". GATT discovery requires at least one
successful master/slave round-trip, which we have not achieved. Tagging now
would be dishonest. The branch stays open; Session 4 picks up from this
commit.

## Memory note

`project_f8a_session3.md` (this session's findings) lands alongside
`project_phase2_nosync.md`. The latter is now stale on H4-H6.
