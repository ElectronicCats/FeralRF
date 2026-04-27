# F8A Session 4 — Task 3: Sniffle CENTRAL A/B against CH573

**Date:** 2026-04-27
**Hardware:** board #1 (CatSniffer 504B32) re-flashed with `sniffle_cc1352p7_1M.hex`
(`/home/sabas/Documents/electroniccats/CatSniffer-Tools/catsnifferv2/release_board-v3.x-v2.0.0/`),
then restored to FeralRF (md5 `2134985c1ecdd6bffb6b345618cfb85e`, verified post-restore
with a 248-packet BLE scan).
**Peer:** CH573 `DC:32:62:8D:E1:09` (`PwnPet_C81F`), public address.

## Result — Sniffle CENTRAL connects, sustains, and exchanges LL packets

`/tmp/f8a-s4-sniffle-initiator-stdout.log` (588 lines, ~12 s of session):

- **Connection setup:** clean transition `INITIATING from STATIC` →
  `CENTRAL from INITIATING` immediately after ADV_IND on ch 37 (RSSI -55 dBm).
- **Connection sustained:** event counter advanced 0 → 192+ over the run
  (~6 s of connection time before timeout).
- **Bidirectional traffic:** 110 events with packets logged. 27 of those
  carry `LL_PING_RSP` from the peripheral (peer → central) — i.e. the slave
  hears our master and replies.
- **RSSI envelope:** peer responses arrive at -55 to -65 dBm consistently —
  CH573 is well within link budget on this antenna.

Connection setup snippet:
```
Timestamp: 0.021524  Length: 25  RSSI: -55  Channel: 37  PHY: 1M  CRC: 0x43FAE2
Ad Type: ADV_IND
ChSel: 0 TxAdd: 0 RxAdd: 0 Ad Length: 23
AdvA: DC:32:62:8D:E1:09 (Public)
TRANSITION: CENTRAL from INITIATING
Timestamp: 0.037416  Length:  2  RSSI: -61  Channel: 12  PHY: 1M  CRC: 0x5E47F1  Event: 0
```

## What this falsifies

- **"CH573 is the wrong exit-criterion peer"** — falsified. CH573 connects
  cleanly to a working master.
- **"The CatSniffer antenna can't sustain a CH573 link"** — falsified. RSSI
  -55 to -65 dBm with bidirectional traffic for 6+ seconds.
- **"BLE 4.2 peer + CSA#1 fallback intrinsically incompatible with our SDK
  setup"** — falsified at the SDK level. Sniffle uses the same SDK family on
  the same chip and connects fine.

## What this confirms

**The bug is 100 % in FeralRF's config of `CMD_BLE5_INITIATOR` / `CMD_BLE5_MASTER`,
or in how we feed `accessAddress` / `crcInit` / `hopIncrement` between the two
phases.** Hardware, peer, antenna, RF stack all proven good.

## Channel sequence — gold reference for our CSA#1 implementation

Sniffle's first 30 captured events (every-other event due to slave-latency=1):

| Event | Channel | `(N+1) * 12 mod 37` | Match |
|-------|---------|---------------------|-------|
| 0     | 12      | 12                  | ✅    |
| 2     | 36      | 36                  | ✅    |
| 4     | 23      | 23                  | ✅    |
| 6     | 10      | 10                  | ✅    |
| 8     | 34      | 34                  | ✅    |
| 10    | 21      | 21                  | ✅    |
| 12    | 8       | 8                   | ✅    |

Sniffle is using **CSA#1** with the exact formula we already implemented in
`ble_conn_mgr.c:270`:
```c
chan = (uint8_t)(((uint32_t)(s_event_counter + 1u) * st->hopIncrement) % 37u);
```

This means the algorithm is right. The problem is one or more of the
*inputs* we hand to it.

## Three precise next-experiment candidates (Session 5 first pass)

The Task 2 evidence (master TX'd, slave silent) plus this gold reference
narrows the hunt to the parameter level:

1. **`hopIncrement` mismatch.** What value did we put in our CONNECT_IND
   (per `ble_conn.c`'s `randint(5, 16)`-style randomization)? Is the SDK
   keeping that value, or overwriting it? Same question we asked of
   `WinOffset` in Session 3 (where the SDK *did* override). Telemetry: log
   `s_state.hopIncrement` immediately post-`BleConn_initiate` and after the
   first master event.

2. **`accessAddress` / `crcInit` snapshot timing.** `BleConn_initiate` reads
   these from `s_ll_data` after the SDK runs `CMD_BLE5_INITIATOR`. If the
   SDK *also* writes back the AA / CRC the slave actually saw on the wire
   (analogous to `WinOffset`), and we read before that write, our master
   would TX with the AA/CRC we *meant to send* but the slave is decoding
   with what it *received* — invariably a mismatch when randomized.

3. **First-event off-by-one.** `s_event_counter` starts at 0 in
   `BleConnMgr_init`. CSA#1 first data channel is `(0+1)*hopIncrement mod 37`.
   The slave's BLE 4.2 stack expects that exact formula. If our
   `BleConnMgr_start` increments before scheduling event 0, our first hop
   lands on `(1+1)*hopIncrement mod 37` → CH573 listens on ch X, we TX on ch
   X+hopIncrement, miss.

The cheapest test: dump `s_state.hopIncrement`, `s_state.accessAddr`, and
`s_state.crcInit` via a new debug command (or extend `conn_status`) and
compare against the wire CONNECT_IND bytes from a Sniffle pcap of our own
attempt. Whichever differs is the bug.

## Verdict

**Hardware, antenna, peer, RF stack, and CSA#1 algorithm choice are all
correct.** The bug is a parameter mismatch between our master config and
what the slave (CH573) is computing — almost certainly one of
`hopIncrement`, `accessAddr`, `crcInit`, or first-event index.

F8A still open; no `v2.0-f8a` tag. Next session has a sharply scoped target.
