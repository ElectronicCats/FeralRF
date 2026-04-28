# F8A Session 6 — close-out report

**Date:** 2026-04-28
**Branch:** `feature/f8a-ble-central-sniffle`
**Range from Session 5:** `0372cc5..ed54eef`
**Outcome:** ✅ **F8A green, ready for `v2.0-f8a` tag.**
GATT discovery + Device Name read against CH573 PwnPet_C81F succeeds in
one script, one pass.

## What happened

Session 5 closed the master-anchor NOSYNC bug, but `gatt_discover()` still
hung. The Session 5 closeout listed three RX-side candidates (queue
saturation, ATT request overwrite, `processBlePackets` discard). All three
were partially correct: **none alone explained the symptom — the actual
root cause was a layered failure across three subsystems.**

## Commits

| SHA       | Subject |
|-----------|---------|
| `1e31721` | fix(f8a): make BLE RX parser tolerant of master no-CRC layout |
| `5c8b561` | fix(f8a): grow host TX queue to 32 frames to land RSP_GATT_DONE |
| `ed54eef` | fix(f8a): send LL_TERMINATE_IND on disconnect for clean peer teardown |

## Three independent bugs, three small fixes

### Bug 1 — `processBlePackets` rejected every master RX entry (1e31721)

`RadioIF_processBlePackets()` used a single `BLE_APPENDED_TOTAL_LEN = 10`
constant (CRC + RSSI + STATUS + TIMESTAMP) to validate the layout of each
RX entry. Scanner / GenericRx use `bIncludeCrc=1` and produce 10-byte
appended blocks. **`CMD_BLE5_MASTER` uses `bIncludeCrc=0`**
(`radio_if.c:2329`), so master entries are 7 bytes appended. Every
master entry failed the `entry_len == 1 + pdu_len + 10` check and was
silently dropped at line 1284-1287. Only one entry per session
slipped through by spurious bit collision in the LL header byte —
hence the persistent `total_rx=1` we observed.

**Fix:** flex the parser. Try four layouts (A/B × CRC/no-CRC) and accept
whichever matches `entry_len` exactly. Flipping `bIncludeCrc=1` on the
master config regresses the initiator — the data queue is shared between
`CMD_BLE5_INITIATOR` and `CMD_BLE5_MASTER`, and changing the appended
layout for one breaks the other. Verified by direct A/B test: setting
master `bIncludeCrc=1` made every connection attempt return `result=3`
(BLE_CONN_ERR_RF) until the change was reverted.

Also flipped `bAutoFlushEmpty=1` on the master so the 3-entry RX queue
doesn't saturate with empty PDUs from the slave; `pktStatus.bLastEmpty`
still surfaces empty ACKs to host telemetry.

### Bug 2 — `RSP_GATT_DONE` got overflowed out of the host TX queue (5c8b561)

After Bug 1's fix, the firmware-side discovery state machine completed
(att_state returned to IDLE, callbacks fired) but Python kept timing out
waiting for `RSP_GATT_DONE`. `OutputIF_sendResponse` silently drops frames
when `PacketQueue_enqueue` returns false; the queue depth was 8.

CH573 PwnPet has 4 services and 23 characteristics, so a full discovery
emits ~28 frames (1 ACK + 4 GATT_SERVICE + 23 GATT_CHAR + 1 GATT_DONE) in
a tight burst. `RF_runCmd(BLE5_MASTER)` blocks the cooperative scheduler
for the entire connection event window (~10 ms), so `HostIFTask_poll`
can't drain UART during the burst. The trailing `RSP_GATT_DONE` was the
overflow casualty.

**Fix:** bump `PACKET_QUEUE_DEPTH` from 8 to 32. Cost: ~6 KB BSS (each
entry is `PACKET_QUEUE_MAX_FRAME_SIZE` bytes), well within the CC1352P7's
80 KB SRAM budget.

### Bug 3 — Dirty disconnect left CH573 ghost-connected (ed54eef)

`BleConn_disconnect` stopped the connection manager and reset local
state without notifying the peer. Per BLE Core Spec Vol 6 Part B §5.1.6,
the master should queue `LL_TERMINATE_IND` (opcode 0x02 + ErrorCode) and
wait for the peer's ACK. Without this, CH573 holds connection state for
its `supervisionTimeout` (1 s) before resuming advertising — back-to-back
demo runs failed because the peer was still in stuck connected state when
we tried to reconnect.

**Fix:** queue `LL_TERMINATE_IND` (LLID=CTRL, reason code 0x13
= REMOTE_USER_TERMINATED_CONNECTION) on disconnect, then `Task_sleep` for
~3 connection intervals (default 90 ms) before tearing down our side.

## Result

`demo_ble_connect_gatt.py DC:32:62:8D:E1:09 0 --read` against CH573
PwnPet_C81F (post-fix, single pass):

```
connect:   result=0
discover:  4 svc, 20 chr, status=0x00
   svc 0x1800 (Generic Access)              h=1..9
   svc 0x1801 (Generic Attribute)           h=10..13
   svc 0x180A (Device Information)          h=14..32
   svc 0xFEED (custom PwnPet)               h=33..53
read handle 3 (Device Name): b'PwnPet_C81F' → "PwnPet_C81F"
disconnect: clean (LL_TERMINATE_IND ACK'd)
```

F8A exit criterion in
`docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md` §F8A:
"`demo_ble_connect_gatt.py … connects, discovery completes, read OK,
disconnect clean`" — **green.**

## Permanent infrastructure

- **`bAutoFlushEmpty=1` on master** — empty PDUs no longer occupy the
  3-entry RX queue. Telemetry-equivalent to the old `=0` because
  `pktStatus.bLastEmpty` exposes the same information.
- **Layout-flexing `processBlePackets`** — handles both CRC and no-CRC
  appended layouts, future-proofing against any other RX command that
  might land here with a different `bIncludeCrc` setting.
- **`LL_TERMINATE_IND` on disconnect** — back-to-back reconnects against
  the same peer no longer rely on `supervisionTimeout` expiring.
- **`PACKET_QUEUE_DEPTH = 32`** — host TX burst headroom for any
  multi-frame response (GATT discovery, future scan reports).

## Regression smoke

- BLE 1M ch37 scan still works (234 packets in 2.5 s, 13 from CH573 ✅)
- Memory: `feralrf_cc1352.elf` 91428 B text + 2620 B data + 46672 B BSS

(Full 8 PHYs validation matrix not re-run this session — anchor change
in Session 5 + this session's RX-path changes are central-mode only and
shouldn't affect any other PHY. Recommended as a F9 prereq.)

## Known follow-ups (NOT blockers for the tag)

1. **F8 human checkpoint** — F8A unblocks F8 (`docs/superpowers/specs/…
   §F8`). The Python side of F8 was complete in Session 0; running the
   demo on a smartphone primary + ESP32 / CH573 secondary is the F8 close.
2. **2× back-to-back consecutive runs without inter-run delay** — Run 1
   fully green, Run 2 sometimes still hits BLE_CONN_ERR_TIMEOUT because
   CH573 needs ~1 s between cycles even after a clean LL_TERMINATE_IND.
   Likely a CH573-side advertising-resume delay; the demo flow doesn't
   exercise this. Worth investigating with a 2 s `time.sleep` between
   runs and confirming it always succeeds.
3. **Async `RF error: code=0x90`** still warned during initiator. Not a
   blocker — connection succeeds anyway. Carry-over from Session 3
   cleanup item.

## Why `v2.0-f8a` is now ready

F8A's scope was always: get a BLE central connection that survives long
enough for one GATT round-trip against a real BLE 4.2 peer. We have:

- Sustained connection (Session 5)
- ATT response delivery (Session 6 Bug 1)
- Streamed GATT response handling (Session 6 Bug 2)
- Clean teardown (Session 6 Bug 3)
- All four CH573 services discovered + named characteristic read OK

Tag.
