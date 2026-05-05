# F20.a.1.b — NOSYNC Debug via Slave Telemetry — Design

**Date:** 2026-05-04
**Branch:** `feature/f20a1-peripheral-read` (continues from `2060967` / tag `v2.0-f20.a.1-partial`)
**Phase:** F20.a.1.b — instrumentation-first debug pass for the slave-side NOSYNC blocker carried over from F20.a.1.

---

## Problem

F20.a.1 closed partial: connection establishes (central CONN_RESULT 0x00) but the slave receives 0 packets in the data state and the central disconnects with reason 0x22 (LL_REASON_SUPERVISION_TIMEOUT) at exactly t≈1.0s. Three timing-side mitigations (HW timestamp from CONNECT_IND, `SLAVE_AO_TARG=10000` look-back, anchor catch-up) did not change the outcome. We do not yet know whether the failure is in PARSING (wrong AA / CRCInit / hopIncrement / winOffset extracted from CONNECT_IND) or in RADIO BEHAVIOR (right params but wrong channel / wrong timing / RX queue management).

This phase adds the instrumentation needed to distinguish the two axes, then iterates fixes from concrete signals.

## Goal

Smoke V2 passes. All slave-parsed fields match the central's actual CONNECT_IND values, the slave receives at least one packet from the master, and the smoke V1 GATT path completes (services ≥ 2, name == `FERAL_GATT`, test == `HELLO_FERAL`).

## Architecture

A single new debug command exposes a snapshot of the slave's parsed CONNECT_IND values plus a fixed-depth ring buffer of per-event RX stats. The host's smoke V2 harness queries the peripheral with this command after the connection drops, queries the central with the existing `CMD_DEBUG_CONN_PARAMS` (F8A telemetry), and diffs the two field-by-field. If the diff reveals a parser bug, we fix the parser. If parsing matches but the ring has `nRxOk=0` across all events, we fix the radio side.

The command + response IDs follow the F8A telemetry numbering pattern (0x47/0x48 → 0xA8/0xA9). The next free pair is **`CMD_DEBUG_SLAVE = 0x49`** → **`RSP_DEBUG_SLAVE = 0xAA`**.

## Wire format

`RSP_DEBUG_SLAVE` payload (max 298 B = 26 header + 16 entries × 17 B):

### Header (26 B) — snapshot at `BleConnMgr_startSlave`
```
offset  size  field
   0     4    accessAddr        (u32 LE)  ← from CONNECT_IND body[0..3]
   4     4    crcInit           (u32 LE)  ← body[4..6] in low 24 bits, high byte 0
   8     2    winOffset_125us   (u16 LE)  ← body[8..9]
  10     2    hopInterval_125us (u16 LE)  ← body[10..11]
  12     2    latency           (u16 LE)
  14     2    supervTimeout_10ms (u16 LE)
  16     1    hopIncrement
  17     4    connectIndEndRat  (u32 LE)  ← HW timestamp + airtime-after-AA
  21     4    firstAnchorRat    (u32 LE)  ← computed s_slave_anchor_rat at startSlave
  25     1    count                       ← number of valid ring entries (0..16)
```

### Ring entry (17 B per entry) — appended end of each `BleConnMgr_pollSlave` iteration
```
offset  size  field
   0     2    event_counter     (u16 LE)
   2     1    chan
   3     4    anchor_rat        (u32 LE)  ← anchor for this event (post catch-up)
   7     4    actual_start      (u32 LE)  ← RF_getCurrentTime() right before RadioIF_bleSlave
  11     2    status            (u16 LE)  ← Ble5_0_cmdBle5Slave.status raw value
  13     1    nRxOk
  14     1    nRxNok
  15     1    nRxIgnored
  16     1    pktStatus                   ← packed bitfield from output.pktStatus
```

Ring depth = 16 entries. Older entries overwrite when the ring saturates; `count` saturates at 16 so the host knows how many slots are valid (head walks 0..15 mod 16).

## Components

### Firmware

- `firmware/cc1352/include/protocol.h` — add `CMD_DEBUG_SLAVE 0x49u` and `RSP_DEBUG_SLAVE 0xAAu` next to existing F8A debug IDs.
- `firmware/cc1352/include/ble_conn_mgr.h` — define
  ```c
  #define BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH 16u

  typedef struct {
      uint16_t event_counter;
      uint8_t  chan;
      uint32_t anchor_rat;
      uint32_t actual_start_rat;
      uint16_t status;
      uint8_t  nRxOk;
      uint8_t  nRxNok;
      uint8_t  nRxIgnored;
      uint8_t  pktStatus;
  } BleConnMgr_DbgSlaveEntry;

  void BleConnMgr_getDbgSlaveSnapshot(BleConnMgr_SlaveParams *out_params,
                                       uint32_t *out_first_anchor_rat);
  uint8_t BleConnMgr_getDbgSlaveRing(BleConnMgr_DbgSlaveEntry *out_buf, uint8_t max_entries);
  ```
- `firmware/cc1352/src/ble_conn_mgr.c`:
  - Add static state: `s_dbg_slave_params_snapshot`, `s_dbg_slave_first_anchor`, `s_dbg_slave_ring[16]`, `s_dbg_slave_head`, `s_dbg_slave_count`.
  - In `BleConnMgr_startSlave`: snapshot `params` and the computed first anchor; reset ring head/count.
  - In `BleConnMgr_pollSlave`: after `RadioIF_bleSlave` returns and stats are populated, write a new ring entry capturing `event_counter`, `chan` (computed pre-event), `anchor_rat` (post catch-up), `actual_start_rat` (captured immediately before `RadioIF_bleSlave`), the `status` returned by `RadioIF_bleSlave`, and the `stats` fields. Advance head; saturate count at depth.
  - Provide accessor functions that copy snapshot/ring into caller-owned buffers (no exposing internals).
- `firmware/cc1352/src/command_processor.c` — new `case CMD_DEBUG_SLAVE` mirror of `CMD_DEBUG_CONN_PARAMS`. Validates `payload_len == 0`, calls the two accessors, packs the wire format above, calls `send_response(RSP_DEBUG_SLAVE, seq, rsp, total_len)`. Refuse with `ERR_NOT_CONNECTED`-equivalent if `BleConnMgr_getDbgSlaveSnapshot` returns "no slave session captured" (header reads back zeros).

### Python

- `python/feralrf/enums.py` — add `Command.DEBUG_SLAVE = 0x49` and `Response.DEBUG_SLAVE = 0xAA` in the existing `# Diagnostics` and matching response sections. Add `Command.DEBUG_SLAVE` to `EXPERIMENTAL_COMMANDS` (it is debug-only and not part of the stable API).
- `python/feralrf/radio.py` — add a `SlaveDbgResult` dataclass and `Radio.debug_slave() -> SlaveDbgResult`:
  ```python
  @dataclass
  class SlaveDbgEntry:
      event_counter: int
      chan: int
      anchor_rat: int
      actual_start_rat: int
      status: int
      n_rx_ok: int
      n_rx_nok: int
      n_rx_ignored: int
      pkt_status: int

  @dataclass
  class SlaveDbgResult:
      access_addr: int
      crc_init: int
      win_offset: int
      hop_interval: int
      latency: int
      superv_timeout: int
      hop_increment: int
      connect_ind_end_rat: int
      first_anchor_rat: int
      entries: list[SlaveDbgEntry]
  ```
  Sends `CMD_DEBUG_SLAVE`, reads `RSP_DEBUG_SLAVE`, parses the wire format, returns the dataclass.

### Smoke V2

- `python/examples/smoke_f20a1_b_diag.py` — same threading model as smoke V1 (peripheral thread + central foreground), but on connection completion it queries:
  - peripheral: `radio.debug_slave()` → slave snapshot + ring
  - central: existing `CMD_DEBUG_CONN_PARAMS` query (write a small helper if not already in `feralrf.radio`).

  Then runs the diff and prints a side-by-side table:
  ```
  Field                Slave             Central           Match
  accessAddr           0x12345678        0x12345678        ✓
  crcInit              0x123456          0x123456          ✓
  hopIncrement         7                 7                 ✓
  winOffset            5                 5                 ✓
  hopInterval          24                24                ✓
  supervTimeout        100               100               ✓
  ```

  Then the ring buffer summary:
  ```
  Slave RX ring (last 10 events):
   evt  chan  anchor_rat  actual_start  Δ      status  nRxOk  nRxNok  nRxIgnored  pktStatus
   1    18    0x4f3220    0x4f3220       0     0x1A03  0      0       0           0x00
   2    25    0x4fff20    0x4fff20       0     0x1A03  0      0       0           0x00
   ...
  ```

  Pass = `(all field matches) AND (any entry.nRxOk > 0) AND (smoke V1 GATT criteria)`.

## Validation

The smoke harness yields one of three outcomes per iteration:

1. **Field mismatch.** One or more parsed values diverge from central. Fix the parser (likely a byte offset or endianness bug). The mismatched field tells us exactly where to look.

2. **Fields match, ring all `nRxOk=0`.** Parsing is correct; the radio is silent. Examine `chan`, `anchor_rat`, `actual_start_rat - anchor_rat` (post-catch-up timing), `status`. If `actual_start - anchor` consistently > some threshold, timing is still wrong. If channel hop progression doesn't match what the master would compute (we know master's hopIncrement now), CSA#1 formula is wrong on slave side. If status reports a specific RF Core error, that's the lead.

3. **Fields match, ring has `nRxOk > 0` somewhere.** Slave is partly working. Inspect which event(s) succeeded vs which failed. Likely a maxRxPktLen / RX queue management issue. Slave may need to call `RadioIF_bleResetRxQueue()` between events.

Iteration is one fix per commit on the existing branch.

## Closing criteria

Smoke V2 passes all three pass conditions. At that point:
1. Drop the `-partial` suffix: retag the new HEAD as `v2.0-f20.a.1` (the original `v2.0-f20.a.1-partial` tag stays for history).
2. FF merge `feature/f20a1-peripheral-read` into `main`.
3. Memory entry replaces `project_f20a1_partial.md` with `project_f20a1_done.md`.

## Out of scope

- F20.a.2 (Write request + HVN/Indicate). Cannot start until F20.a.1.b unblocks slave RX.
- F20.b (dynamic GATT table from host).
- Debug telemetry retention beyond the F20.a.1.b debug cycle. The new command is in `EXPERIMENTAL_COMMANDS` and may be removed (or moved behind a build flag) after the bug is fixed. Decision deferred to a separate cleanup pass.

## Risks

- **Ring buffer race.** `BleConnMgr_pollSlave` writes to the ring; `command_processor` reads it via the accessor. They run on different tasks (RfTask writes, host thread reads). Single-word ARM stores are atomic on Cortex-M4; we accept one stale-read iteration in the worst case. Same pattern as F8d's pending-DC fields, which have been validated wire-level. Documented in the accessor.
- **298-byte response.** F8A `DEBUG_TIMING` already returns 1+18×N bytes (up to 577 B). The host frame buffer is sized for these, so no path issue expected. Smoke V2 will exercise the largest case.
- **Snapshot of `BleConnMgr_SlaveParams` includes a pointer-to-struct field?** No — `BleConnMgr_SlaveParams` is plain values only (verified by reading the typedef in `ble_conn_mgr.h`). Memcpy is safe.
- **Multiple iteration cycles required.** Each cycle is build + dual flash + smoke run, ~3-4 minutes. We expect 2-5 iterations. Acceptable cost.
