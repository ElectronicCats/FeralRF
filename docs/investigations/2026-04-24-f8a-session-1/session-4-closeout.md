# F8A Session 4 — close-out report

**Date:** 2026-04-27
**Branch:** `feature/f8a-ble-central-sniffle`
**Range from Session 3:** `b238930..b2bb432`
**Outcome:** ❌ **F8A still open** — but the search space collapsed from
"unknown reason for `nTxEntryDone == 0`" to "one of three named
parameter mismatches between FeralRF's master config and CH573".
`v2.0-f8a` tag deferred to Session 5.

## Tasks executed

| Task | Subject                                        | Verdict |
|------|-----------------------------------------------|---------|
| 0    | Preflight (branch, ports, CH573)               | ✅ Bench ready, CH573 alive (34 ADV_IND/233 pkts in 3 s). |
| 1    | Sniffle pinned-channel passive capture         | ⏭️ Skipped — Task 2 + Task 3 made it redundant; left as pending fallback. |
| 2    | Per-event `pOutput` stats in `RSP_DEBUG_TIMING` | ✅ `nTx == 1` every event, all RX counters 0 → master TX'd, slave silent. Wire entry now 18 B (was 13 B); depth 14. |
| 3    | Sniffle CENTRAL A/B against CH573              | ✅ Sniffle CENTRAL connected, sustained 110+ events with 27 LL_PING_RSP from peer at -55/-65 dBm. Bug is 100 % in our config. |
| 4    | Slow `connInterval` / `supervTimeout`           | ⏭️ Skipped — Task 3 proved bug is parameter-mismatch, not timing-window-width. Left pending. |
| 5    | This close-out                                  | ✅ Session 4 wrap. |

## What landed in this branch

| Commit    | Subject |
|-----------|---------|
| `8bc13b0` | docs(f8a): land Session 4 plan + preflight |
| `2bf7e54` | feat(f8a): expose per-event pOutput stats in RSP_DEBUG_TIMING |
| `b2bb432` | investigate(f8a): Task 3 — Sniffle CENTRAL A/B against CH573 |
| (this)    | docs(f8a): Session 4 close-out |

## Evidence summary

**Task 2** instrumented `RadioIF_bleCentral` to expose `pOutput.nTx`,
`nRxOk`, `nRxNok`, `nRxIgnored`, and a packed `pktStatus` byte per
event. The first capture against CH573 (default 30 ms / 1 s) showed
**every NOSYNC event with `nTx == 1` and all RX counters zero**. This
falsifies the session-3-closeout hypothesis (a) "RF core exits the
master state machine before the TX state". The master IS putting one
packet on the air per event — almost certainly the auto-empty PDU
since `nTxEntryDone == 0` (the queued data PDU never gets ACK'd).

**Task 3** re-flashed the same board #1 with stock
`sniffle_cc1352p7_1M.hex` and ran `initiator.py` against the same CH573
peer over the same antenna. Sniffle CENTRAL connected immediately
(`TRANSITION: CENTRAL from INITIATING` after a single ADV_IND on ch 37)
and sustained the link through event 192+ with bidirectional traffic —
27 `LL_PING_RSP` packets from the peripheral at -55 to -65 dBm. Channel
sequence followed the exact same CSA#1 formula we already implement in
`ble_conn_mgr.c:270`: ev 0 → ch 12, ev 2 → ch 36, ev 4 → ch 23,
ev 6 → ch 10, ev 8 → ch 34, ev 10 → ch 21, ev 12 → ch 8 — all matching
`(N+1) * 12 mod 37` with `hopIncrement = 12`. Then board #1 was
restored to FeralRF (md5 `2134985c1ecdd6bffb6b345618cfb85e`) and verified
with a 248-packet BLE scan.

## What the two experiments together imply

The bug is **not** in:

- Hardware / antenna / RF calibration (Sniffle on the same board works).
- The peer (CH573 happily connects to Sniffle).
- Our CSA#1 algorithm (Sniffle uses the same formula and our
  implementation in `ble_conn_mgr.c:270` matches its output bit-for-bit).
- The RF core's master state machine (it executes through the TX state —
  Task 2 evidence `nTx == 1`).
- Anchor timing in any way that would benefit from a wider window
  (`bTimeStampValid == 0` and `nTx == 1` are independent of how much
  slack the slave has — the master TX simply doesn't reach the slave's
  receiver, period).

The bug **is** in one of these inputs we feed to `CMD_BLE5_MASTER`:

1. **`hopIncrement` value.** What we put in the on-wire CONNECT_IND
   may differ from what we use in `BleConnMgr_poll` for the hop
   computation. Same class of bug as the WinOffset SDK-overrides-host
   issue Session 3 already documented.
2. **`accessAddress` / `crcInit` snapshot timing.** We read these from
   `s_ll_data` after `CMD_BLE5_INITIATOR` runs. If the SDK writes back
   the AA / CRC the slave actually saw on the wire (analogous to
   WinOffset), and we snapshot before that write, master TX uses
   wrong-AA / wrong-CRC packets the slave silently drops.
3. **First-event off-by-one alignment.** `BleConnMgr_init` sets
   `s_event_counter = 0`; CSA#1 first data channel is
   `(0+1)*hopIncrement mod 37`. If `BleConnMgr_start` advances
   `s_event_counter` before scheduling event 0, our first hop lands on
   `(1+1)*hopIncrement mod 37` while CH573 listens on
   `(0+1)*hopIncrement mod 37` — and then every subsequent event is
   one slot ahead.

All three are consistent with: master TX'd + peer-silent, slave never
saw a valid packet on its expected channel/AA/CRC.

## Why this session can't tag `v2.0-f8a`

`v2.0-f8a`'s exit criterion is `demo_ble_connect_gatt` connect +
discovery + read against CH573. Connection still drops within 1
supervisionTimeout (`BLE_DONE_NOSYNC` every event, no slave reply). No
honest tag.

## Session 5 — sharply scoped first task

The cheapest experiment that decisively narrows from three candidates
to one: **expose `s_state.hopIncrement`, `s_state.accessAddr`, and
`s_state.crcInit` in a debug response right after
`BleConn_initiate` returns**. Compare those three values against what
Sniffle (board #2 in `--hop` mode) saw on the wire in our CONNECT_IND.

- All three match the wire → bug is the off-by-one (candidate 3).
- AA or CRC differs from wire → bug is candidate 2; fix is a
  re-snapshot after a small delay or after a known SDK callback.
- `hopIncrement` differs → bug is candidate 1; same fix pattern.

Estimated cost: one new firmware command (`CMD_DEBUG_CONN_PARAMS`,
~50 lines), a paired Sniffle pcap, < 30 minutes. Almost certainly
flips F8A green within the same session.

## Status of permanent infrastructure landed

- **`RSP_DEBUG_TIMING` 18-byte entries** (Task 2). The `(nTx, nRxOk,
  bTimeStampValid)` triple is now the canonical telemetry for
  diagnosing master-RX-side issues; subsequent sessions should keep
  using it instead of guessing.
- **Snapshot hex file pattern** (Task 3). The flow "snapshot, re-flash
  with reference firmware, A/B, restore from snapshot, verify with
  smoke scan" is reusable for any future "is the bug in our firmware?"
  question.

## Open items (not blockers for Session 5 entry)

- Task 1 (pinned-channel passive Sniffle) is left as a pending fallback;
  the firmware AA-fix detour it required is also not done. If Session 5
  needs to confirm a particular event TX is on the right channel, the
  script `python/examples/lab/f8a_session4_sniffle_pinned.py` from the
  plan is still un-implemented. Easy to revive if needed.
- Task 4 (slow `connInterval` / `supervTimeout`) similarly skipped. If a
  future session wants the wider window for any reason, the plan still
  describes the exact wire layout extension.
- The `Async RF error: code=0x2F` warning seen in the preflight `init()`
  call is a leftover from Session 3's NOSYNC chain. Not a Session 4
  blocker, but worth investigating in Session 5 cleanup.
