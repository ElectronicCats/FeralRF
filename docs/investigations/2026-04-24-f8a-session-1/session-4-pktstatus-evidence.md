# F8A Session 4 — Task 2: per-event pOutput stats evidence

**Date:** 2026-04-27
**Capture:** `/tmp/f8a-s4-pktstatus.json` (FeralRF on board #1, target CH573 `DC:32:62:8D:E1:09`, default 30 ms / 1 s)
**Firmware:** `b238930` + Session-4 telemetry extension (this commit)

## What the new telemetry exposes

`RSP_DEBUG_TIMING` ring entries grew from 13 to 18 wire bytes. Per event we now record:

| Field        | Source                          | Captures |
|--------------|---------------------------------|----------|
| `numSent`    | `pOutput->nTxEntryDone`         | TX queue entries that were ACK'd |
| `nTx`        | `pOutput->nTx`                  | Total TX (incl. auto-empty + retransmissions) |
| `nRxOk`      | `pOutput->nRxOk`                | Good-CRC RX packets (not ignored) |
| `nRxNok`     | `pOutput->nRxNok`               | CRC-error RX packets |
| `nRxIgnored` | `pOutput->nRxIgnored`           | Good-CRC RX packets ignored due to repeated SN |
| `pktStatus`  | `pOutput->pktStatus` (packed)   | bTimeStampValid, bLastCrcErr, bLastIgn, bLastEmpty, bLastCtrl, bLastMd, bLastAck |

Depth capped at 14 entries to stay under `PROTOCOL_MAX_PAYLOAD = 255` (1 + 14*18 = 253).

## Per-event dump (last 14 of ~33 events before supervTimeout)

```
 ev status sent  nTx rxOk rxNok rxIgn pktSt  bits
 19 0x1402    0    1    0     0     0 0x00 (none)
 20 0x1402    0    1    0     0     0 0x00 (none)
 21 0x1402    0    1    0     0     0 0x00 (none)
 22 0x1402    0    1    0     0     0 0x00 (none)
 23 0x1402    0    1    0     0     0 0x00 (none)
 24 0x1402    0    1    0     0     0 0x00 (none)
 25 0x1402    0    1    0     0     0 0x00 (none)
 26 0x1402    0    1    0     0     0 0x00 (none)
 27 0x1402    0    1    0     0     0 0x00 (none)
 28 0x1402    0    1    0     0     0 0x00 (none)
 29 0x1402    0    1    0     0     0 0x00 (none)
 30 0x1402    0    1    0     0     0 0x00 (none)
 31 0x1402    0    1    0     0     0 0x00 (none)
 32 0x1402    0    1    0     0     0 0x00 (none)
```

`conn_result=0  events=0  last_status=0x1402 (NOSYNC)  connected=False  connTime=73656187`

(`events=0` is a stale read from `BleConnMgr_getEventCount()` after the connection
already collapsed — the ring buffer is the authoritative event count here.)

## What this falsifies and what it confirms

**Falsified:** "Master state machine never executes / RF core exits before TX."
The hypothesis from session-3-closeout § Remaining mystery (option (a): "the RF
core is exiting the master state machine before the TX state") is **wrong**.
`nTx == 1` on every event proves the master command transitions through the TX
state and emits exactly one packet per event (almost certainly the auto-empty
PDU, since `nTxEntryDone == 0` means our queued data PDU never got ACK'd).

**Confirmed:** "Master TX'd, slave silent." The signature `nTx == 1, all RX
counters == 0, bTimeStampValid == 0` matches the second branch from the
session-3-closeout: queued entries stay PENDING because the slave never
acknowledges them. The slave never replies at all — not even with a CRC-broken
packet that would have shown up in `nRxNok`.

## What's left — three remaining causes

The peer hears nothing despite our master TX'ing once per event. Three
mechanisms produce this exact pattern:

1. **Channel mismatch.** Our CSA hop computation lands on a channel that's not
   what CH573 expects for the same event index. CH573 (BLE 4.2) uses CSA#1.
   We forced CSA#1 fallback in Session 3 with `(N+1) * hopIncrement mod 37`.
   If the formula is right but the parameters (`hopIncrement`, `event_counter`
   alignment with peer's first-event) are off-by-one, every event lands on the
   wrong channel.
2. **AA / CRC-init mismatch on data channel.** We pass `accessAddr` and
   `crcInit` to `RadioIF_bleCentral` from `s_state` — populated by the SDK
   after `CMD_BLE5_INITIATOR`. If the SDK rewrote one of those during the
   initiator run and we read back a stale snapshot, master TX uses values the
   peer rejects.
3. **Per-event TX queue auto-empty quirk.** `nTxEntryDone == 0` could also
   mean the SDK is using the auto-empty PDU instead of our queued PDU
   (because of `bAutoEmpty` in seqStat or because TX queue entries are
   structurally invalid). Worth auditing `TXQueue_insert` + `tx_queue.c`.

## Next experiments (drop-in replacements for plan Task 1 / Task 3)

- **Task 1 revival, with parameters now chosen by the diagnosis:** pin Sniffle
  on board #2 to channel `((event_idx + 1) * hopIncrement) mod 37` for a
  specific event we observe. If we see master TX there → CSA computation is
  right and (1) is falsified, leaving (2) or (3). If we don't see master TX
  there → CSA is computing the wrong channel.
- **Read back `accessAddr` and `crcInit` from `s_state` immediately after
  initiator and log them.** Compare against what Sniffle saw in the on-wire
  CONNECT_IND (Session 2 log `sniffle-capture-post-inita-fix.log` shows
  `AA: random` — we have the value there). If they differ, that's the bug.
- **Audit `tx_queue.c` against the data-entry format CMD_BLE5_MASTER expects.**
  An invalid queue entry would explain `nTxEntryDone == 0` even on TX.

## Verdict

**The mystery from Session 3 has shifted, not closed.** Master TX is happening.
Slave hears nothing. Three precise candidate causes remain, all cheaper to test
than the "master never executes" path. F8A still open; no `v2.0-f8a` tag.
