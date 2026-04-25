# F8A Session 2 — close-out report

**Date:** 2026-04-24
**Branch:** `feature/f8a-ble-central-sniffle`
**Range from Session 1:** `b3f365e..HEAD`
**Boards:**
- #1 BF:F1 (504B32) — FeralRF under test on `/dev/ttyACM0`.
- #2 C1:82 (565932) — Sniffle CC1352P7 1M flashed (Sniffle from catnip alias `sniffle`), broken antenna RSSI ≈ −90/−97 dBm, but enough for the on-air oracle.

## Single committed code change

**InitA static-random subtype bug.** `firmware/cc1352/src/ble_conn.c:107` had the
hardcoded own-address MSB set to `0xAA` (top 2 bits = `10` = Reserved-For-Future-Use
per BLE 5.0 Vol 6 Part B §1.3.2.1). With `deviceAddrType=1` (random), the SDK
transmits the address as random in the CONNECT_IND PDU; the peer's stack
silently rejects RFU subtypes. Sniffle on board #2 confirmed the bug
(`InitA: AA:BB:CC:DD:EE:01 (RFU)` in `sniffle-capture-broken-inita.log`).

Fixed by changing MSB from `0xAA` → `0xCA` (top 2 bits = `11` = Static Random).
Sniffle confirms the fix on the wire (`InitA: CA:BB:CC:DD:EE:01 (Static)` in
`sniffle-capture-post-inita-fix.log`).

This is **a real bug** — the previous default address would have failed against
any spec-compliant peer, not just CH573. Worth committing regardless of the
remaining NOSYNC.

## What Sniffle proved

1. **CONNECT_IND lands on air** with valid InitA after fix:
   `Length: 36, ChSel: 1, TxAdd: 1 (Static), AdvA: DC:32:62:8D:E1:09 (Public),
   AA: random, CRCInit: random, Interval: 24 (30 ms), Timeout: 100 (1 s),
   Hop: 5..16 random, ChM: all 37`. Byte-identical to the Python oracle
   (`feralrf.ble.connect_ind`).
2. **CH573 ACCEPTS the CONNECT_IND post-fix.** Sniffle observes
   `TRANSITION: DATA from STATIC` immediately after our CONNECT_IND, then
   `TRANSITION: STATIC from DATA` ~290 ms later. CH573 stops advertising for
   ~290 ms (≈10 connection events) — exactly the window during which it is
   waiting for our master TX.
3. **CH573 hears nothing during those 290 ms** and drops back to advertising.
   Our master events are firing (events climbing 8→22→34 in `conn_status`)
   but landing outside the peer's listening window.
4. **`bDynamicWinOffset=1`** causes the SDK to override the WinOffset value
   in our `pConnectReqData`. Sniffle saw `WinOffset: 11` on air vs the random
   5..15 we encoded — confirming the SDK is the WinOffset authority.

## Three failed timing experiments (with oracle)

| Experiment | Change | Outcome |
|------------|--------|---------|
| **H1 — skip RF_cancelCmd+RF_flushCmd** (`radio_if.c`) | Drop the pre-initiator hygiene to keep the radio warm | Neutral. Same 17-event NOSYNC pattern. Reverted. |
| **H2 — WinSize 3 → 10** (`ble_conn.c`) | Widen the peer's listening window to 12.5 ms | Peer kept the connection alive longer (events climbed to 34) but still 100 % NOSYNC. Reverted. |
| **H3 — alternate anchor formula + `bDynamicWinOffset=0` + `WinOffset=0`** (`ble_conn_mgr.c`, `ble_conn.c`) | `nextHopTime = connTime + transmitWindowDelay + WinOffset*1.25 + interval - AO_TARG`; deterministic on-air WinOffset confirmed by Sniffle (`WinOffset: 0`) | Still NOSYNC. Reverted. |

The combination "Sniffle interprets SDK `connectTime` as first anchor and uses
`connTime + interval - AO_TARG`" works for Sniffle on the same hardware but
not for us. Some delta in RF state, calibration, or driver invocation order
remains unaccounted for.

## Lessons / claim retraction

The Session 1 close-out claimed *"`events>0` is a bonus delivery, what would
have been Session 2's first telemetry milestone now happens for free."* That
claim is partially walked back: events do tick, but they are firing into a
peer that **never accepted the connection in Session 1** (because of the
RFU InitA). Now that InitA is valid, the peer accepts but our master TX
timing misses every window. The honest framing is: we found one bug, fixed
it, and uncovered a second.

## What survives Session 2

- New default own address `01:EE:DD:CC:BB:CA` (commit pending).
- Sniffle on board #2 ready as oracle for Session 3.
- Two pcaps + two raw Sniffle logs in `docs/investigations/2026-04-24-f8a-session-1/`
  for any future regression / comparison.

## Recommended Session 3 entry — telemetry first, no more blind tweaks

The cheap experiments are exhausted. Next session should add per-event RAT
timestamps from inside `BleConnMgr_poll` (start, end, status) as a binary
debug response, capture both Sniffle's wallclock and our RAT, and compute
the actual offset between our master TX and CH573's listening windows from
DATA. The right anchor formula will then be one calculation away.

Concretely: `RSP_DEBUG_TIMING (0xA8)` carrying a ring buffer of the last
N events: `(startRAT u32) (endRAT u32) (status u16) (numSent u8)` × N.
Host-side dump correlates with Sniffle pcap timestamps post-hoc.

After fixing the master-TX timing, Session 3 also handles GATT round-trip,
ICall cleanup, regression matrix, and tag `v2.0-f8a` — i.e. everything that
Session 1 originally outlined as "Session 3".
