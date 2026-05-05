# F20.a.1.c — Internal-State Trace Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend `RSP_DEBUG_SLAVE` with 6 firmware-internal trace fields so a single Smoke V2 run pinpoints why `BleConnMgr_startSlave` is never called in the F20.a.1 peripheral handoff path.

**Architecture:** Add 6 module-static counters/snapshots in `radio_if.c` and `command_processor.c`, expose them through getters, and pack them into the existing `RSP_DEBUG_SLAVE` header (26 B → 35 B). The slave-side ring depth drops from 13 → 12 to keep the frame ≤ 255 bytes (`PROTOCOL_MAX_PAYLOAD`). Python parser + dataclass + smoke harness are extended in lockstep. After the trace lands on hardware, one Smoke V2 run reveals the failing layer (TI status, handoff flag, parser entry, queue contents, or loop iteration count) and the next 1–2 commits fix it.

**Tech Stack:** C / TI-RTOS 7 / TI SimpleLink CC13xx SDK 8.30; Python 3 / pyserial; pytest.

**Branch:** continue on `feature/f20a1-peripheral-read` (HEAD `2c75afb`, tag `v2.0-f20.a.1.b-partial`). Partial tags are checkpoints, not merges — do **not** branch off.

**Pre-flight:** working tree has an uncommitted fix in `python/examples/smoke_f20a1_b_diag.py` (adds the missing `connect()` calls in `query_diagnostics`). That diff must be committed before Task 1 — see Task 0.

---

## File Structure

| File | Role | Change |
|---|---|---|
| `firmware/cc1352/include/protocol.h` | Wire format constants | (none — RSP_DEBUG_SLAVE already 0xB3) |
| `firmware/cc1352/include/ble_conn_mgr.h` | Slave-side ring depth | DEPTH `13u` → `12u` |
| `firmware/cc1352/include/radio_if.h` | Public radio-if API | Add `RadioIF_getDbgF21Trace()` getter + struct |
| `firmware/cc1352/src/radio_if.c` | Counters + last_tx_status + adv-iter counter | Increment counters in `RadioIF_extractConnectIndParams`; add iteration counter in `RadioIF_transmitBleAdvLegacy` loop; expose via getter |
| `firmware/cc1352/src/command_processor.c` | Wire-format packing for RSP_DEBUG_SLAVE | Capture `s_peripheral_active` snapshot at handoff entry; pack 9 new bytes into header |
| `python/feralrf/radio.py` | `SlaveDbgResult` dataclass + parser | Add 6 new fields; update min-payload check (26 → 35); update parsing offsets |
| `python/tests/test_radio_debug_slave.py` | Parser unit tests | Update `_build_payload` helper + existing tests; add new test for the 6 trace fields |
| `python/examples/smoke_f20a1_b_diag.py` | Smoke V2 harness | Print the 6 trace fields in the diff section + interpretive table |

The slave debug ring drops from 13 → 12 entries: 35 (header) + 12 × 17 (entries) = 239 ≤ 255 (`PROTOCOL_MAX_PAYLOAD`). One ring slot traded for 9 B of header observability.

---

## Wire Format (header layout, post-extension)

```
Offset  Size  Field                          Source
------  ----  -----------------------------  ----------------------------------------
 0       4   accessAddr                u32   snap.accessAddr (LE)
 4       4   crcInit                   u32   snap.crcInit (LE)
 8       2   winOffset_125us           u16   snap.winOffset_125us (LE)
10       2   hopInterval_125us         u16   snap.hopInterval_125us (LE)
12       2   latency                   u16   snap.latency (LE)
14       2   supervTimeout_10ms        u16   snap.supervTimeout_10ms (LE)
16       1   hopIncrement              u8    snap.hopIncrement
17       4   connectIndEndRat          u32   snap.connectIndEndRat (LE)
21       4   firstAnchorRat            u32   first_anchor (LE)
                  --- existing 26 B header ends here ---
25       2   lastTxStatus              u16   RadioIF trace last_tx_status (LE)
27       1   peripheralActiveAtHandoff u8    command_processor snapshot (0|1)
28       1   extractCallCount          u8    RadioIF trace (saturating)
29       1   extractEntriesSeen        u8    RadioIF trace (saturating)
30       1   extractFirstPduType       u8    RadioIF trace
31       2   advertiseIterations       u16   RadioIF trace (LE, saturating)
                  --- new 9 B trace block ---
33       1   reserved                  u8    pad to align ring to offset 34? NO — see below
34       1   count                     u8    number of ring entries
35      …    entries[count]            …    17 B each, up to 12
```

**Note:** the layout above places `count` at offset 34 (not 33). To avoid an unused reserved byte, pack `count` at offset 33 and start ring entries at offset 34. Final layout used by the implementation:

```
Offset  Size  Field
------  ----  -----------------------------
 0..24      previous header fields (unchanged)
25       2   lastTxStatus              u16 LE
27       1   peripheralActiveAtHandoff u8
28       1   extractCallCount          u8
29       1   extractEntriesSeen        u8
30       1   extractFirstPduType       u8
31       2   advertiseIterations       u16 LE
33       1   count                     u8     ← was at offset 25
34       …   entries[count]            17 B each, up to 12
```

Total = 34 + count×17. With count=12: 34 + 204 = 238 ≤ 255. ✓

The `count` field's offset moved from 25 → 33; that is a breaking change to the parser, which Task 5 handles.

---

## Task 0: Commit the smoke-script connect() fix

**Files:**
- Modify: `python/examples/smoke_f20a1_b_diag.py` (already modified — see `git diff`)

- [ ] **Step 1: Verify the diff is the connect() fix**

Run: `git diff python/examples/smoke_f20a1_b_diag.py`

Expected: shows `+ per.connect()` and `+ cen.connect()` inside `query_diagnostics`, plus a docstring tweak. No other changes.

- [ ] **Step 2: Commit it as a precursor fix**

```bash
git add python/examples/smoke_f20a1_b_diag.py
git commit -m "fix(f20.a.1.b): smoke V2 query_diagnostics opens serial ports

Radio() ctor doesn't open the port — debug_slave/debug_conn_params
were called against a closed serial. Five smoke runs hit this before
even reaching the trace investigation.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 3: Verify tree is clean**

Run: `git status`
Expected: `nothing to commit, working tree clean`. HEAD is one commit ahead of `2c75afb`.

---

## Task 1: Reduce slave ring DEPTH from 13 → 12

**Files:**
- Modify: `firmware/cc1352/include/ble_conn_mgr.h:83`

- [ ] **Step 1: Change the DEPTH constant**

Edit `firmware/cc1352/include/ble_conn_mgr.h` line 81–83:

Old:
```c
/* F20.a.1.b — slave-side per-event ring entry. sizeof==20 (3 B compiler padding);
 * hand-packed wire size is 17 B. Depth 13: 26 B header + 13*17 B = 247 B ≤ PROTOCOL_MAX_PAYLOAD. */
#define BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH 13u
```

New:
```c
/* F20.a.1.c — slave-side per-event ring entry. sizeof==20 (3 B compiler padding);
 * hand-packed wire size is 17 B. Depth 12: 34 B header + 12*17 B = 238 B ≤ PROTOCOL_MAX_PAYLOAD.
 * Header grew from 26 → 34 B in F20.a.1.c (+8 B trace fields), so depth dropped 13 → 12. */
#define BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH 12u
```

- [ ] **Step 2: Build firmware to confirm no other breakage**

Run: `cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -20`
Expected: build completes; only the two existing call sites of `BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH` (in `ble_conn_mgr.c` and `command_processor.c`) recompile.

- [ ] **Step 3: Commit**

```bash
git add firmware/cc1352/include/ble_conn_mgr.h
git commit -m "feat(f20.a.1.c): slave debug ring DEPTH 13 → 12

Make room in RSP_DEBUG_SLAVE for the 9 B trace block landing in the
next commits. Ring rarely fills past a handful of entries during the
~1 s NOSYNC window we're chasing.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 2: Add F21 trace counters and getter in radio_if

**Files:**
- Modify: `firmware/cc1352/include/radio_if.h` (after line 198, before `RadioIF_getRxQueue`)
- Modify: `firmware/cc1352/src/radio_if.c` (top: file-static counters; `RadioIF_extractConnectIndParams`; `RadioIF_transmitBleAdvLegacy`; new getter)

Trace fields — module-static in `radio_if.c`:

| Variable | Type | When set |
|---|---|---|
| `s_dbg_extract_call_count` | `uint8_t` | incremented at top of `RadioIF_extractConnectIndParams` (saturate at 0xFF) |
| `s_dbg_extract_entries_seen` | `uint8_t` | incremented inside the parser's while-loop per FINISHED entry walked (saturate at 0xFF) |
| `s_dbg_extract_first_pdu_type` | `uint8_t` | set ONCE per parser call to `pkt[0] & 0x0F` of the first FINISHED entry |
| `s_dbg_advertise_iterations` | `uint16_t` | incremented each iteration of the F21 ADV loop (saturate at 0xFFFF) |

`s_last_tx_status` already exists at `radio_if.c:170` — reuse, expose via getter.

- [ ] **Step 1: Add getter struct + prototype to radio_if.h**

Insert after line 198 (right after the `RadioIF_extractConnectIndParams` block):

```c
/* F20.a.1.c — internal-state trace exposed via RSP_DEBUG_SLAVE.
 * All counters saturate (no wrap) so a stuck condition reads as 0xFF/0xFFFF
 * rather than as 0. Reset is implicit on each new advertise/extract cycle:
 * extract counters reset on entry to RadioIF_extractConnectIndParams,
 * advertise iterations reset on entry to RadioIF_transmitBleAdvLegacy. */
typedef struct {
    uint16_t lastTxStatus;        /* mirrors s_last_tx_status — last TX cmd->status */
    uint16_t advertiseIterations; /* count of F21 ADV iterations executed in last call */
    uint8_t extractCallCount;     /* times RadioIF_extractConnectIndParams was invoked */
    uint8_t extractEntriesSeen;   /* FINISHED entries the most-recent parser call walked */
    uint8_t extractFirstPduType;  /* (pkt[0] & 0x0F) of the first FINISHED entry seen */
} RadioIF_DbgF21Trace;

void RadioIF_getDbgF21Trace(RadioIF_DbgF21Trace *out);
```

- [ ] **Step 2: Add module-static counters near s_last_tx_status (~line 170)**

Locate the block with `static uint16_t s_last_tx_status = 0u;`. After it, add:

```c
/* F20.a.1.c — internal-state trace counters (see RadioIF_DbgF21Trace). */
static uint8_t s_dbg_extract_call_count = 0u;
static uint8_t s_dbg_extract_entries_seen = 0u;
static uint8_t s_dbg_extract_first_pdu_type = 0u;
static uint16_t s_dbg_advertise_iterations = 0u;
```

- [ ] **Step 3: Instrument RadioIF_extractConnectIndParams (radio_if.c:3071)**

Replace the function body. Old body is at lines 3071–3101. New body — counters reset per call, increment on each FINISHED entry, capture first pdu_type:

```c
bool RadioIF_extractConnectIndParams(BleConnMgr_SlaveParams *out_params) {
    if (out_params == NULL)
        return false;
    if (s_dbg_extract_call_count < 0xFFu) {
        s_dbg_extract_call_count++;
    }
    s_dbg_extract_entries_seen = 0u;
    s_dbg_extract_first_pdu_type = 0xFFu; /* sentinel: no entries seen yet */
    bool first_seen = false;
    while (RadioIF_rfHasPacket()) {
        rfc_dataEntryGeneral_t *entry = s_rf_read_entry;
        uint8_t *pkt = (uint8_t *)&entry->data;
        uint8_t header = pkt[0];
        uint8_t pdu_type = header & 0x0Fu;
        uint8_t length = pkt[1];
        if (!first_seen) {
            s_dbg_extract_first_pdu_type = pdu_type;
            first_seen = true;
        }
        if (s_dbg_extract_entries_seen < 0xFFu) {
            s_dbg_extract_entries_seen++;
        }
        if (pdu_type == 0x5u && length >= 34u) {
            const uint8_t *body = &pkt[2 + 6 + 6];
            out_params->accessAddr = (uint32_t)body[0] | ((uint32_t)body[1] << 8) |
                                     ((uint32_t)body[2] << 16) | ((uint32_t)body[3] << 24);
            out_params->crcInit =
                (uint32_t)body[4] | ((uint32_t)body[5] << 8) | ((uint32_t)body[6] << 16);
            out_params->winOffset_125us = (uint16_t)body[8] | ((uint16_t)body[9] << 8);
            out_params->hopInterval_125us = (uint16_t)body[10] | ((uint16_t)body[11] << 8);
            out_params->latency = (uint16_t)body[12] | ((uint16_t)body[13] << 8);
            out_params->supervTimeout_10ms = (uint16_t)body[14] | ((uint16_t)body[15] << 8);
            out_params->hopIncrement = body[21] & 0x1Fu;
            const uint8_t *ts = &pkt[2 + length];
            uint32_t timestamp = (uint32_t)ts[0] | ((uint32_t)ts[1] << 8) |
                                 ((uint32_t)ts[2] << 16) | ((uint32_t)ts[3] << 24);
            out_params->connectIndEndRat = timestamp + ((uint32_t)length + 5u) * 32u;
            RadioIF_rfConsumeEntry();
            return true;
        }
        RadioIF_rfConsumeEntry();
    }
    return false;
}
```

- [ ] **Step 4: Instrument the F21 advertise loop (radio_if.c:794)**

Locate the loop:
```c
    for (uint16_t i = 0u; i < count; i++) {
        memset(&s_f21_adv_output, 0, sizeof(s_f21_adv_output));
        cmd->status = 0x0000;
```

Reset the counter before the loop, increment inside it. Replace the section at lines 793–795 — find:

```c
    for (uint16_t i = 0u; i < count; i++) {
        memset(&s_f21_adv_output, 0, sizeof(s_f21_adv_output));
        cmd->status = 0x0000;
```

with:

```c
    s_dbg_advertise_iterations = 0u;
    for (uint16_t i = 0u; i < count; i++) {
        if (s_dbg_advertise_iterations < 0xFFFFu) {
            s_dbg_advertise_iterations++;
        }
        memset(&s_f21_adv_output, 0, sizeof(s_f21_adv_output));
        cmd->status = 0x0000;
```

- [ ] **Step 5: Add the getter at the end of radio_if.c**

Append after the closing brace of `RadioIF_extractConnectIndParams` (last function in the file):

```c
void RadioIF_getDbgF21Trace(RadioIF_DbgF21Trace *out) {
    if (out == NULL) {
        return;
    }
    out->lastTxStatus = s_last_tx_status;
    out->advertiseIterations = s_dbg_advertise_iterations;
    out->extractCallCount = s_dbg_extract_call_count;
    out->extractEntriesSeen = s_dbg_extract_entries_seen;
    out->extractFirstPduType = s_dbg_extract_first_pdu_type;
}
```

- [ ] **Step 6: Build firmware and confirm clean**

Run: `cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -30`
Expected: build completes with no warnings related to the new counters/getter.

- [ ] **Step 7: Commit**

```bash
git add firmware/cc1352/include/radio_if.h firmware/cc1352/src/radio_if.c
git commit -m "feat(f20.a.1.c): F21/extract internal-state trace counters

Add 4 module-static trace counters in radio_if (extract call count,
entries-seen, first-pdu-type, advertise-iterations) plus expose
last_tx_status via a unified getter RadioIF_getDbgF21Trace. All
counters saturate so a stuck loop reads 0xFF/0xFFFF rather than 0.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 3: Capture peripheral_active_at_handoff in command_processor

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c` (CMD_BLE_ADV_LEGACY handler around line 1377)
- Modify: `firmware/cc1352/src/command_processor.c` (top of file: add static `s_dbg_peripheral_active_at_handoff`)
- Modify: `firmware/cc1352/src/command_processor.c` (CMD_DEBUG_SLAVE handler around line 1147 — pack new bytes)

- [ ] **Step 1: Add a module-static snapshot near s_peripheral_active**

Locate `static bool s_peripheral_active = false;` at `command_processor.c:183`. Add immediately after it:

```c
/* F20.a.1.c — captured at the exact handoff check after RadioIF_transmitBleAdvLegacy.
 * Lets RSP_DEBUG_SLAVE distinguish "serve_gatt never armed it" vs "armed but
 * cleared between serve_gatt and the handoff check". 0xFF = never reached the
 * handoff (CMD_BLE_ADV_LEGACY did not run since boot or last debug query). */
static uint8_t s_dbg_peripheral_active_at_handoff = 0xFFu;
```

- [ ] **Step 2: Capture the snapshot at the handoff check**

Locate the section at lines 1373–1388:

```c
        /* F20.a.1: if peripheral mode is armed AND ADV exited because of
         * CONNECT_IND, extract params and run the slave event loop.
         * connectIndEndRat is populated by extractConnectIndParams from
         * the HW-appended timestamp (s_f21_bleAdvPar.bAppendTimestamp=1). */
        if (adv_ok && s_peripheral_active) {
```

Insert the capture line immediately before the `if`:

```c
        /* F20.a.1: if peripheral mode is armed AND ADV exited because of
         * CONNECT_IND, extract params and run the slave event loop.
         * connectIndEndRat is populated by extractConnectIndParams from
         * the HW-appended timestamp (s_f21_bleAdvPar.bAppendTimestamp=1). */
        s_dbg_peripheral_active_at_handoff = s_peripheral_active ? 1u : 0u;
        if (adv_ok && s_peripheral_active) {
```

The capture happens unconditionally — even if `adv_ok` is false. That's intentional: we want to know what `s_peripheral_active` was *at handoff*, not what gated entry into the inner block.

- [ ] **Step 3: Extend the RSP_DEBUG_SLAVE wire packing**

Locate the `case CMD_DEBUG_SLAVE:` block at `command_processor.c:1147`. Three things change:

1. Add `#include "radio_if.h"` is already present (verify — should be at top of file).
2. Pull the trace via `RadioIF_getDbgF21Trace` after pulling the existing snapshot.
3. Pack 9 new bytes between offsets 25–33; shift `count` from 25 → 33; ring entries shift base from 26 → 34. Buffer size grows from `26 + DEPTH*17` to `34 + DEPTH*17` (= 34 + 12*17 = 238).

Replace the block from line 1147 to line 1232 (the `return;` ending the case) with:

```c
    case CMD_DEBUG_SLAVE: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }

        BleConnMgr_SlaveParams snap;
        uint32_t first_anchor = 0u;
        BleConnMgr_getDbgSlaveSnapshot(&snap, &first_anchor);

        BleConnMgr_DbgSlaveEntry entries[BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH];
        uint8_t n = BleConnMgr_getDbgSlaveRing(entries, BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH);

        RadioIF_DbgF21Trace trace;
        RadioIF_getDbgF21Trace(&trace);

        /* Wire layout (34 B header + n * 17 B entries):
         *   accessAddr               u32 LE   (4)   off  0
         *   crcInit                  u32 LE   (4)   off  4
         *   winOffset_125us          u16 LE   (2)   off  8
         *   hopInterval_125us        u16 LE   (2)   off 10
         *   latency                  u16 LE   (2)   off 12
         *   supervTimeout_10ms       u16 LE   (2)   off 14
         *   hopIncrement             u8       (1)   off 16
         *   connectIndEndRat         u32 LE   (4)   off 17
         *   firstAnchorRat           u32 LE   (4)   off 21
         *   --- F20.a.1.c trace block (9 B) ---
         *   lastTxStatus             u16 LE   (2)   off 25
         *   peripheralActiveAtHand   u8       (1)   off 27
         *   extractCallCount         u8       (1)   off 28
         *   extractEntriesSeen       u8       (1)   off 29
         *   extractFirstPduType      u8       (1)   off 30
         *   advertiseIterations      u16 LE   (2)   off 31
         *   count                    u8       (1)   off 33
         *   --- entries[n] start at off 34, 17 B each ---
         *     event_counter   u16 LE   (2)
         *     chan            u8       (1)
         *     anchor_rat      u32 LE   (4)
         *     actual_start    u32 LE   (4)
         *     status          u16 LE   (2)
         *     nRxOk           u8       (1)
         *     nRxNok          u8       (1)
         *     nRxIgnored      u8       (1)
         *     pktStatus       u8       (1) */
        uint8_t rsp[34u + BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH * 17u];
        rsp[0] = (uint8_t)(snap.accessAddr & 0xFFu);
        rsp[1] = (uint8_t)((snap.accessAddr >> 8) & 0xFFu);
        rsp[2] = (uint8_t)((snap.accessAddr >> 16) & 0xFFu);
        rsp[3] = (uint8_t)((snap.accessAddr >> 24) & 0xFFu);
        rsp[4] = (uint8_t)(snap.crcInit & 0xFFu);
        rsp[5] = (uint8_t)((snap.crcInit >> 8) & 0xFFu);
        rsp[6] = (uint8_t)((snap.crcInit >> 16) & 0xFFu);
        rsp[7] = (uint8_t)((snap.crcInit >> 24) & 0xFFu);
        rsp[8] = (uint8_t)(snap.winOffset_125us & 0xFFu);
        rsp[9] = (uint8_t)((snap.winOffset_125us >> 8) & 0xFFu);
        rsp[10] = (uint8_t)(snap.hopInterval_125us & 0xFFu);
        rsp[11] = (uint8_t)((snap.hopInterval_125us >> 8) & 0xFFu);
        rsp[12] = (uint8_t)(snap.latency & 0xFFu);
        rsp[13] = (uint8_t)((snap.latency >> 8) & 0xFFu);
        rsp[14] = (uint8_t)(snap.supervTimeout_10ms & 0xFFu);
        rsp[15] = (uint8_t)((snap.supervTimeout_10ms >> 8) & 0xFFu);
        rsp[16] = snap.hopIncrement;
        rsp[17] = (uint8_t)(snap.connectIndEndRat & 0xFFu);
        rsp[18] = (uint8_t)((snap.connectIndEndRat >> 8) & 0xFFu);
        rsp[19] = (uint8_t)((snap.connectIndEndRat >> 16) & 0xFFu);
        rsp[20] = (uint8_t)((snap.connectIndEndRat >> 24) & 0xFFu);
        rsp[21] = (uint8_t)(first_anchor & 0xFFu);
        rsp[22] = (uint8_t)((first_anchor >> 8) & 0xFFu);
        rsp[23] = (uint8_t)((first_anchor >> 16) & 0xFFu);
        rsp[24] = (uint8_t)((first_anchor >> 24) & 0xFFu);
        rsp[25] = (uint8_t)(trace.lastTxStatus & 0xFFu);
        rsp[26] = (uint8_t)((trace.lastTxStatus >> 8) & 0xFFu);
        rsp[27] = s_dbg_peripheral_active_at_handoff;
        rsp[28] = trace.extractCallCount;
        rsp[29] = trace.extractEntriesSeen;
        rsp[30] = trace.extractFirstPduType;
        rsp[31] = (uint8_t)(trace.advertiseIterations & 0xFFu);
        rsp[32] = (uint8_t)((trace.advertiseIterations >> 8) & 0xFFu);
        rsp[33] = n;

        for (uint8_t i = 0u; i < n; i++) {
            uint8_t *p = &rsp[34u + (uint16_t)i * 17u];
            p[0] = (uint8_t)(entries[i].event_counter & 0xFFu);
            p[1] = (uint8_t)((entries[i].event_counter >> 8) & 0xFFu);
            p[2] = entries[i].chan;
            p[3] = (uint8_t)(entries[i].anchor_rat & 0xFFu);
            p[4] = (uint8_t)((entries[i].anchor_rat >> 8) & 0xFFu);
            p[5] = (uint8_t)((entries[i].anchor_rat >> 16) & 0xFFu);
            p[6] = (uint8_t)((entries[i].anchor_rat >> 24) & 0xFFu);
            p[7] = (uint8_t)(entries[i].actual_start_rat & 0xFFu);
            p[8] = (uint8_t)((entries[i].actual_start_rat >> 8) & 0xFFu);
            p[9] = (uint8_t)((entries[i].actual_start_rat >> 16) & 0xFFu);
            p[10] = (uint8_t)((entries[i].actual_start_rat >> 24) & 0xFFu);
            p[11] = (uint8_t)(entries[i].status & 0xFFu);
            p[12] = (uint8_t)((entries[i].status >> 8) & 0xFFu);
            p[13] = entries[i].nRxOk;
            p[14] = entries[i].nRxNok;
            p[15] = entries[i].nRxIgnored;
            p[16] = entries[i].pktStatus;
        }

        send_response(RSP_DEBUG_SLAVE, seq, rsp, (uint16_t)(34u + (uint16_t)n * 17u));
        return;
    }
```

- [ ] **Step 4: Build and confirm**

Run: `cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -30`
Expected: clean build. The `rsp` buffer size went from `26+13*17=247` to `34+12*17=238`, both well under PROTOCOL_MAX_PAYLOAD=255.

- [ ] **Step 5: Commit**

```bash
git add firmware/cc1352/src/command_processor.c
git commit -m "feat(f20.a.1.c): pack 9 B trace block into RSP_DEBUG_SLAVE

Header grows 26 → 34 B; count moves to off 33; ring entries to off 34.
peripheral_active_at_handoff captured unconditionally at the handoff
check so a false 'adv_ok' doesn't mask flag-clear bugs. lastTxStatus +
extractCallCount + extractEntriesSeen + extractFirstPduType +
advertiseIterations come from RadioIF_getDbgF21Trace.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 4: Extend Python SlaveDbgResult dataclass

**Files:**
- Modify: `python/feralrf/radio.py` (`SlaveDbgResult` dataclass at line 242)
- Modify: `python/feralrf/radio.py` (`debug_slave` parser at line 971)

- [ ] **Step 1: Add 6 new fields to SlaveDbgResult**

Locate `SlaveDbgResult` at `radio.py:242`. Replace with:

```python
@dataclass
class SlaveDbgResult:
    """Slave-side diagnostic dump from CMD_DEBUG_SLAVE (F20.a.1.b + .c trace)."""

    access_addr: int
    crc_init: int
    win_offset: int
    hop_interval: int
    latency: int
    superv_timeout: int
    hop_increment: int
    connect_ind_end_rat: int
    first_anchor_rat: int
    # F20.a.1.c — internal-state trace
    last_tx_status: int
    peripheral_active_at_handoff: int
    extract_call_count: int
    extract_entries_seen: int
    extract_first_pdu_type: int
    advertise_iterations: int
    entries: list[SlaveDbgEntry]
```

- [ ] **Step 2: Update debug_slave parser to read the new layout**

Locate `def debug_slave(self) -> SlaveDbgResult:` at `radio.py:971`. Replace the body's parsing section (the `if len(payload) < 26` check through the final `return SlaveDbgResult(...)`):

```python
    def debug_slave(self) -> SlaveDbgResult:
        """F20.a.1.b/.c — query slave-side diagnostic dump.

        Returns the snapshot of CONNECT_IND values parsed by the slave plus
        the F20.a.1.c trace (last_tx_status, peripheral_active_at_handoff,
        extract_call_count, extract_entries_seen, extract_first_pdu_type,
        advertise_iterations) plus a ring of the most recent per-event RX
        stats. Used by the smoke V2 harness to diff slave-parsed values
        against the central's actuals and to spot-check radio behavior.
        Debug-only API; not in the stable command set.
        """
        self._send_command(Command.DEBUG_SLAVE, b"")
        cmd_id, _seq, payload = self._read_response(
            timeout=2.0, expected={Response.DEBUG_SLAVE, Response.ERROR}
        )
        if cmd_id == Response.ERROR:
            raise CommandError("DEBUG_SLAVE failed", payload[0] if payload else 0)
        if len(payload) < 34:
            raise ProtocolError(f"DEBUG_SLAVE payload too short: {len(payload)} bytes")
        access_addr = int.from_bytes(payload[0:4], "little")
        crc_init = int.from_bytes(payload[4:8], "little")
        win_offset = int.from_bytes(payload[8:10], "little")
        hop_interval = int.from_bytes(payload[10:12], "little")
        latency = int.from_bytes(payload[12:14], "little")
        superv_timeout = int.from_bytes(payload[14:16], "little")
        hop_increment = payload[16]
        connect_ind_end_rat = int.from_bytes(payload[17:21], "little")
        first_anchor_rat = int.from_bytes(payload[21:25], "little")
        last_tx_status = int.from_bytes(payload[25:27], "little")
        peripheral_active_at_handoff = payload[27]
        extract_call_count = payload[28]
        extract_entries_seen = payload[29]
        extract_first_pdu_type = payload[30]
        advertise_iterations = int.from_bytes(payload[31:33], "little")
        count = payload[33]
        entries = []
        for i in range(count):
            base = 34 + i * 17
            if base + 17 > len(payload):
                break
            entries.append(
                SlaveDbgEntry(
                    event_counter=int.from_bytes(payload[base : base + 2], "little"),
                    chan=payload[base + 2],
                    anchor_rat=int.from_bytes(payload[base + 3 : base + 7], "little"),
                    actual_start_rat=int.from_bytes(payload[base + 7 : base + 11], "little"),
                    status=int.from_bytes(payload[base + 11 : base + 13], "little"),
                    n_rx_ok=payload[base + 13],
                    n_rx_nok=payload[base + 14],
                    n_rx_ignored=payload[base + 15],
                    pkt_status=payload[base + 16],
                )
            )
        return SlaveDbgResult(
            access_addr=access_addr,
            crc_init=crc_init,
            win_offset=win_offset,
            hop_interval=hop_interval,
            latency=latency,
            superv_timeout=superv_timeout,
            hop_increment=hop_increment,
            connect_ind_end_rat=connect_ind_end_rat,
            first_anchor_rat=first_anchor_rat,
            last_tx_status=last_tx_status,
            peripheral_active_at_handoff=peripheral_active_at_handoff,
            extract_call_count=extract_call_count,
            extract_entries_seen=extract_entries_seen,
            extract_first_pdu_type=extract_first_pdu_type,
            advertise_iterations=advertise_iterations,
            entries=entries,
        )
```

- [ ] **Step 3: Verify the dataclass change does not break imports**

Run: `cd python && python -c "from feralrf.radio import SlaveDbgResult; print(SlaveDbgResult.__dataclass_fields__.keys())"`
Expected: prints the 16 field names ending in `entries`. No ImportError.

- [ ] **Step 4: Don't commit yet — Task 5 updates the matching tests**

---

## Task 5: Update unit tests for the new wire format

**Files:**
- Modify: `python/tests/test_radio_debug_slave.py`

The existing tests build payloads by concatenating bytes — they need:
- `_build_payload` to emit the 9 B trace block before `count`
- Each `snap` dict entry to provide trace defaults (or use a helper)
- `test_truncated_header_raises` to use `b"\x00" * 20` (still short of 34)
- `test_count_truncated_by_payload` to write `count` at offset 33 not 25
- A new `test_trace_fields_round_trip` that asserts the 6 new fields parse

- [ ] **Step 1: Update `_build_payload` helper**

Locate `def _build_payload(...)` at the top of the test file. Replace with:

```python
def _build_payload(snapshot: dict, entries: List[dict]) -> bytes:
    """Build a synthetic RSP_DEBUG_SLAVE payload for testing (F20.a.1.c layout)."""
    buf = bytearray()
    buf.extend(snapshot["access_addr"].to_bytes(4, "little"))
    buf.extend(snapshot["crc_init"].to_bytes(4, "little"))
    buf.extend(snapshot["win_offset"].to_bytes(2, "little"))
    buf.extend(snapshot["hop_interval"].to_bytes(2, "little"))
    buf.extend(snapshot["latency"].to_bytes(2, "little"))
    buf.extend(snapshot["superv_timeout"].to_bytes(2, "little"))
    buf.append(snapshot["hop_increment"])
    buf.extend(snapshot["connect_ind_end_rat"].to_bytes(4, "little"))
    buf.extend(snapshot["first_anchor_rat"].to_bytes(4, "little"))
    # F20.a.1.c trace block (9 B) — fields default to 0 if absent
    buf.extend(snapshot.get("last_tx_status", 0).to_bytes(2, "little"))
    buf.append(snapshot.get("peripheral_active_at_handoff", 0))
    buf.append(snapshot.get("extract_call_count", 0))
    buf.append(snapshot.get("extract_entries_seen", 0))
    buf.append(snapshot.get("extract_first_pdu_type", 0))
    buf.extend(snapshot.get("advertise_iterations", 0).to_bytes(2, "little"))
    buf.append(len(entries))
    for e in entries:
        buf.extend(e["event_counter"].to_bytes(2, "little"))
        buf.append(e["chan"])
        buf.extend(e["anchor_rat"].to_bytes(4, "little"))
        buf.extend(e["actual_start_rat"].to_bytes(4, "little"))
        buf.extend(e["status"].to_bytes(2, "little"))
        buf.append(e["n_rx_ok"])
        buf.append(e["n_rx_nok"])
        buf.append(e["n_rx_ignored"])
        buf.append(e["pkt_status"])
    return bytes(buf)
```

- [ ] **Step 2: Update `test_truncated_header_raises`**

Find the test (around line 154):

Old:
```python
    def test_truncated_header_raises(self):
        from feralrf.exceptions import ProtocolError

        radio, fake = _radio_with_fake_serial()
        # 10 bytes — short of the 26-byte header
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=b"\x00" * 10)
        with pytest.raises(ProtocolError, match="too short"):
            radio.debug_slave()
```

New:
```python
    def test_truncated_header_raises(self):
        from feralrf.exceptions import ProtocolError

        radio, fake = _radio_with_fake_serial()
        # 20 bytes — short of the 34-byte F20.a.1.c header
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=b"\x00" * 20)
        with pytest.raises(ProtocolError, match="too short"):
            radio.debug_slave()
```

- [ ] **Step 3: Update `test_count_truncated_by_payload` to write at offset 33**

Find the test at the bottom of the file. The hand-built payload appends `count` after `first_anchor_rat`. We need to insert the 9 B trace block first. Easiest fix: just use the updated `_build_payload` helper plus a manual truncation:

Replace the test entirely:

```python
    def test_count_truncated_by_payload(self):
        """If header says count=5 but only 2 entries' worth of bytes follow,
        parser should return the 2 actually-present entries (silent truncation)."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0xAAAAAAAA,
            "crc_init": 0x00BBBBBB,
            "win_offset": 0,
            "hop_interval": 24,
            "latency": 0,
            "superv_timeout": 100,
            "hop_increment": 7,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "last_tx_status": 0x1404,
            "peripheral_active_at_handoff": 1,
            "extract_call_count": 1,
            "extract_entries_seen": 1,
            "extract_first_pdu_type": 0x05,
            "advertise_iterations": 1,
        }
        # _build_payload writes len(entries) as count; we want count=5 with only 2 entries on the wire.
        two_entries = [
            {
                "event_counter": 1, "chan": 5,
                "anchor_rat": 0, "actual_start_rat": 0,
                "status": 0, "n_rx_ok": 0, "n_rx_nok": 0,
                "n_rx_ignored": 0, "pkt_status": 0,
            },
            {
                "event_counter": 2, "chan": 5,
                "anchor_rat": 0, "actual_start_rat": 0,
                "status": 0, "n_rx_ok": 0, "n_rx_nok": 0,
                "n_rx_ignored": 0, "pkt_status": 0,
            },
        ]
        full = bytearray(_build_payload(snap, two_entries))
        # Override the count byte at offset 33 to claim 5 entries, but leave only 2 on wire
        full[33] = 5
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=bytes(full))
        result = radio.debug_slave()
        assert len(result.entries) == 2
        assert result.entries[0].event_counter == 1
        assert result.entries[1].event_counter == 2
```

- [ ] **Step 4: Add a new test for the trace fields**

Append a new test method to `class TestDebugSlaveParser`:

```python
    def test_trace_fields_round_trip(self):
        """F20.a.1.c — assert the 6 new trace fields parse from the wire."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0xDEADBEEF,
            "crc_init": 0x00ABCDEF,
            "win_offset": 5,
            "hop_interval": 24,
            "latency": 0,
            "superv_timeout": 100,
            "hop_increment": 7,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "last_tx_status": 0x1404,
            "peripheral_active_at_handoff": 1,
            "extract_call_count": 3,
            "extract_entries_seen": 5,
            "extract_first_pdu_type": 0x05,
            "advertise_iterations": 42,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.last_tx_status == 0x1404
        assert result.peripheral_active_at_handoff == 1
        assert result.extract_call_count == 3
        assert result.extract_entries_seen == 5
        assert result.extract_first_pdu_type == 0x05
        assert result.advertise_iterations == 42
        # Sentinel: never-set should read back as 0xFF for first_pdu_type
        # (firmware initializes it to 0xFF on entry to extract); here we set it
        # explicitly, but verify default/round-trip works for both.
        snap2 = dict(snap)
        snap2["extract_first_pdu_type"] = 0xFF
        payload2 = _build_payload(snap2, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload2)
        result2 = radio.debug_slave()
        assert result2.extract_first_pdu_type == 0xFF
```

- [ ] **Step 5: Run the full test file**

Run: `cd python && source .venv/bin/activate && pytest tests/test_radio_debug_slave.py -v`
Expected: all 4 prior tests + 1 new test PASS (5 PASS).

- [ ] **Step 6: Run the entire suite to confirm no regression**

Run: `cd python && source .venv/bin/activate && pytest 2>&1 | tail -10`
Expected: same total as `v2.0-f20.a.1.b-partial` (590 passed / 1 skipped) modulo the +1 new test → 591 passed / 1 skipped.

- [ ] **Step 7: Commit Task 4 + Task 5 together**

```bash
git add python/feralrf/radio.py python/tests/test_radio_debug_slave.py
git commit -m "feat(f20.a.1.c): SlaveDbgResult + parser for 9 B trace block

Add 6 trace fields to SlaveDbgResult; parser reads layout 34 B header
(was 26 B). Min-payload check moves 26 → 34. Count moves to offset 33
and entries to offset 34. Tests updated; new test_trace_fields_round_trip
locks in the round-trip semantics including the 0xFF 'never set' sentinel
for extract_first_pdu_type.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 6: Print trace fields in the smoke harness

**Files:**
- Modify: `python/examples/smoke_f20a1_b_diag.py` (extend the diagnostic-dumps section)

- [ ] **Step 1: Add a trace-print helper near `diff_table`**

Insert a new function after `diff_table` (around line 169 in the current file):

```python
def trace_table(slave):
    """F20.a.1.c — print the 6 internal-state trace fields with interpretive
    notes. The output of this table is the primary diagnostic signal for
    Smoke V2: whichever line disagrees with the expected value tells us
    which firmware layer is failing. Returns the list of printed lines."""
    lines = ["Field                          Value           Interpretation"]
    lines.append("-" * 78)

    pa = slave.peripheral_active_at_handoff
    pa_note = {
        0xFF: "handoff never reached (CMD_BLE_ADV_LEGACY did not run)",
        0: "flag was CLEARED before handoff (serve_gatt didn't arm OR cleared mid-flight)",
        1: "flag survived (handoff entered the inner block)",
    }.get(pa, "unexpected value")
    lines.append(f"peripheral_active_at_handoff   0x{pa:02X}            {pa_note}")

    ts = slave.last_tx_status
    ts_note = {
        0x0000: "never set (no TX ran since boot/last query)",
        0x1404: "BLE_DONE_CONNECT (CONNECT_IND received — happy path)",
        0x140A: "BLE_DONE_CONNECT_CHSEL0 (CONNECT_IND, legacy ch sel)",
        0x1400: "BLE_DONE_OK (ADV completed, no CONNECT_IND)",
        0x1FFF: "BLE_ERROR_RXBUF",
    }.get(ts, "other — check rf_ble_mailbox.h")
    lines.append(f"last_tx_status                 0x{ts:04X}          {ts_note}")

    ec = slave.extract_call_count
    ec_note = "parser was never invoked (handoff bypassed the call)" if ec == 0 else \
              f"parser ran {ec} time(s)"
    lines.append(f"extract_call_count             {ec:<3}             {ec_note}")

    es = slave.extract_entries_seen
    es_note = "queue was empty when parser ran" if es == 0 else \
              f"parser walked {es} FINISHED entry(ies)"
    lines.append(f"extract_entries_seen           {es:<3}             {es_note}")

    pt = slave.extract_first_pdu_type
    pt_note = {
        0xFF: "no FINISHED entry seen (sentinel)",
        0x05: "CONNECT_IND (expected)",
        0x00: "ADV_IND (peer responded with adv, not connect)",
        0x06: "ADV_SCAN_IND",
        0x03: "SCAN_REQ",
        0x04: "SCAN_RSP",
    }.get(pt, "other — check BT Core Spec PDU types")
    lines.append(f"extract_first_pdu_type         0x{pt:02X}            {pt_note}")

    ai = slave.advertise_iterations
    lines.append(
        f"advertise_iterations           {ai:<5}           "
        f"{'loop completed full count (no CONNECT_IND break)' if ai >= 5000 else 'broke early — likely CONNECT_IND'}"
    )

    return lines
```

- [ ] **Step 2: Call the helper from `main()`**

Locate the section in `main()` after `diff_table` is printed (around line 227). Replace:

```python
    print("\n--- Field diff (slave parsed vs central actual) ---")
    all_match, lines = diff_table(slave, central)
    for line in lines:
        print(line)

    print("\n--- Slave RX ring (oldest first) ---")
```

with:

```python
    print("\n--- Field diff (slave parsed vs central actual) ---")
    all_match, lines = diff_table(slave, central)
    for line in lines:
        print(line)

    print("\n--- F20.a.1.c internal-state trace ---")
    for line in trace_table(slave):
        print(line)

    print("\n--- Slave RX ring (oldest first) ---")
```

- [ ] **Step 3: Compile-check**

Run: `cd python && source .venv/bin/activate && python -m py_compile examples/smoke_f20a1_b_diag.py`
Expected: no output (success).

- [ ] **Step 4: Commit**

```bash
git add python/examples/smoke_f20a1_b_diag.py
git commit -m "test(f20.a.1.c): print 6 trace fields in smoke V2 with interpretive notes

trace_table maps each value to the firmware layer it implicates so a
single Smoke V2 run diagnoses the NOSYNC blocker without needing extra
introspection.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 7: HUMAN CHECKPOINT — flash + Smoke V2 run

**This task is interactive and cannot be done by an agent.**

- [ ] **Step 1: Build artifacts**

Run: `cd firmware/cc1352 && cmake --build build -j$(nproc) 2>&1 | tail -10`
Expected: `firmware/cc1352/build/feralrf_cc1352.hex` (≈374 KB) regenerated.

- [ ] **Step 2: Detect ports** (USB enum shifts between sessions)

Run: `cd python && source .venv/bin/activate && python -m catnip devices`
Expected: lists 2 CatSniffers. Pick which is peripheral and which is central.

- [ ] **Step 3: Flash both boards (.hex, retry 2× before giving up per workflow rules)**

For each port (replace `/dev/ttyACM<n>` with the actual data ports):

```bash
python -m catnip flash -p /dev/ttyACM<n> -f firmware/cc1352/build/feralrf_cc1352.hex
```

If the first attempt fails, retry once before asking the user to power-cycle.

- [ ] **Step 4: Run Smoke V2**

```bash
cd python && source .venv/bin/activate && \
PYTHONPATH=. python examples/smoke_f20a1_b_diag.py \
    --peripheral-port /dev/ttyACM<peripheral> \
    --central-port /dev/ttyACM<central>
```

- [ ] **Step 5: Capture and interpret the trace block**

Read the new `--- F20.a.1.c internal-state trace ---` section. Whichever line's interpretation column points to a failing layer is the root cause.

Decision tree (after Step 5):

| Symptom | Failing layer | Likely fix |
|---|---|---|
| `peripheral_active_at_handoff == 0xFF` | `CMD_BLE_ADV_LEGACY` never ran the handoff path | Investigate why `RadioIF_transmitBleAdvLegacy` returned with `adv_ok=false` and the `if (adv_ok && ...)` was skipped — but capture is unconditional, so this means the smoke flow doesn't hit the case at all. Check smoke harness ordering. |
| `peripheral_active_at_handoff == 0` | `s_peripheral_active` was cleared before handoff | Trace serve_gatt → advertise_ind. Likely a Python flow bug where serve_gatt ACK arrived but a reset cleared firmware state, OR a firmware path clearing the flag (e.g., disconnect path). |
| `peripheral_active_at_handoff == 1` AND `extract_call_count == 0` | Handoff entered, but parser was never called | The `RadioIF_extractConnectIndParams` short-circuit. Check the `adv_ok && s_peripheral_active` condition — `adv_ok` might be false even when the peripheral was active. |
| `extract_call_count >= 1` AND `extract_entries_seen == 0` | Parser ran, but queue was empty | F21 `pRxQ` assignment isn't taking effect, OR the radio terminated before writing the entry to FINISHED state. Check `s_rf_data_queue` initialization. |
| `extract_entries_seen >= 1` AND `extract_first_pdu_type != 0x05` | Queue has entries but not CONNECT_IND | TI is writing other PDU types into the queue (e.g., scan requests). Adjust parser filter or queue assignment. |
| `last_tx_status` is 0x1400 (BLE_DONE_OK) | F21 ADV completed without CONNECT_IND | Central never sent CONNECT_IND or it was dropped. Investigate central log; advertise_iterations should be == count. |
| `last_tx_status == 0x1404` AND `extract_first_pdu_type == 0x05` | Happy path — Smoke V2 should PASS | If smoke still FAILs, check ring entries for `nRxOk == 0` (slave anchor still off). |

- [ ] **Step 6: Stop here. Report findings.**

This is the planned checkpoint. The smoke output's trace block is the primary deliverable of F20.a.1.c. Ship the findings to the user before any fix work.

---

## Task 8: Apply the fix indicated by the trace (1–2 commits)

**This task's contents depend entirely on Task 7's output. The plan covers the most-likely scenarios with sketches; the actual fix requires reading the trace.**

- [ ] **Step 1: Re-read the trace and pick the matching row from the decision tree above**

- [ ] **Step 2: Implement the indicated fix**

Each scenario has a known starting point:

- **`s_peripheral_active == 0` mid-flight:** `grep -n "s_peripheral_active = false" firmware/cc1352/src/command_processor.c` to find every path that clears the flag. Identify which one fired. Likely candidates: a stray reset path, the disconnect callback, an error path.

- **`extract_call_count == 0`:** the `if (adv_ok && s_peripheral_active)` is short-circuiting. Check `RadioIF_transmitBleAdvLegacy` return — `adv_ok` might be false because `executeTxCommand` returned false on the iteration that received CONNECT_IND. Cross-check with `last_tx_status`: if 0x1404, then RF saw CONNECT_IND but `executeTxCommand` returned false. Look at iter-3's `1f24adf` defensive workaround.

- **`extract_entries_seen == 0`:** queue was empty. `s_f21_bleAdvPar.pRxQ = &s_rf_data_queue` ran (we know from `c9b6813`), but the radio either didn't write the entry to FINISHED status before our read, OR the queue was reset between the radio write and our parse call. Add a brief `Task_sleep(1)` between TX completion and parse, OR confirm `entry->status == DATA_ENTRY_FINISHED`.

- **`extract_first_pdu_type != 0x05`:** TI is writing other PDU types. Increase the parser to skip non-0x05 entries (already does). Confirm the actual value seen and search TI docs / `rfc_dataEntryGeneral_t` headers.

- [ ] **Step 3: Rebuild + re-flash + re-run Smoke V2**

Same as Task 7 Steps 1–4.

- [ ] **Step 4: Confirm Smoke V2 PASS**

Expected console output:
```
[PASS] all parsed fields match central actuals
[PASS] slave received >=1 packet from master (any nRxOk>0)
[PASS] GATT path: services>=2, name+test correct
[PASS] Smoke V2 overall
```

- [ ] **Step 5: Commit the fix(es)**

```bash
git add <files-changed>
git commit -m "fix(f20.a.1.c): <root-cause description>

Trace from Smoke V2 showed <which field> at <value>, indicating <layer>.
<one-line on the underlying mechanism>.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

---

## Task 9: Run pre-commit + full test suite

- [ ] **Step 1: Run pre-commit**

Run: `cd /home/sabas/Documents/electroniccats/FeralRF && pre-commit run --all-files`
Expected: all hooks pass. Per project workflow, never skip with `--no-verify`.

- [ ] **Step 2: Run full Python test suite**

Run: `cd python && source .venv/bin/activate && pytest 2>&1 | tail -5`
Expected: ≥591 passed / 1 skipped (590 baseline + 1 new from Task 5).

- [ ] **Step 3: Re-run Smoke V2 once more for stability**

Same Smoke V2 command as Task 7 Step 4. Should PASS again. If flaky, that's a separate issue (file as F20.a.1.d) — do not block close.

---

## Task 10: Close-out — retag, FF merge, memory update

- [ ] **Step 1: Retag**

Drop both `-partial` and `.b` from the tag:

```bash
git tag -d v2.0-f20.a.1.b-partial
git tag -d v2.0-f20.a.1-partial
git tag -a v2.0-f20.a.1 -m "F20.a.1 — peripheral mode wire-level CLOSED.

Closes both F20.a.1 (peripheral) and the F20.a.1.b/.c diagnostic
sub-iterations. Smoke V2 PASS end-to-end: peripheral advertise →
central connect → CONNECT_IND parse → slave event loop → GATT
discovery (services ≥ 2, name == FERAL_GATT, test == HELLO_FERAL).

Supersedes v2.0-f20.a.1-partial and v2.0-f20.a.1.b-partial."
```

- [ ] **Step 2: Verify tag**

Run: `git tag -l "v2.0-f20*" -n1`
Expected: only `v2.0-f20.a.1` listed; the two -partial tags are gone.

- [ ] **Step 3: FF merge to main**

```bash
git checkout main
git merge --ff-only feature/f20a1-peripheral-read
```

If FF fails because main moved, abort and consult user — do **not** attempt a non-FF merge unless the user approves.

- [ ] **Step 4: Push branch + tag** (only if user approves; do not push without confirmation per safety protocol)

Confirm with user. If yes:

```bash
git push origin main
git push origin v2.0-f20.a.1
```

- [ ] **Step 5: Update memory**

Write `project_f20a1_done.md` replacing both `project_f20a1_partial.md` and `project_f20a1b_partial.md`. Use this template (fill `<…>`):

```markdown
---
name: project_f20a1_done
description: F20.a.1 (BLE peripheral mode) closed wire-level YYYY-MM-DD on tag v2.0-f20.a.1. Smoke V2 PASS end-to-end (CONNECT_IND parse + slave event loop + GATT). Three layered bugs fixed during F20.a.1.b + .c iterations. Branch FF'd into main.
type: project
---
F20.a.1 closed YYYY-MM-DD commit <hash>, tag v2.0-f20.a.1.

Branch feature/f20a1-peripheral-read FF'd into main.

Closed bugs (in order found):
- F20.a.1.b iter1: read-head walk (commit b3d08a1)
- F20.a.1.b iter2: F21 pRxQ NULL (commit c9b6813)
- F20.a.1.b iter4: BLE_DONE_CONNECT 0x1404 (commit 2c75afb)
- F20.a.1.c: <root cause described in 1 sentence> (commit <new>)

Smoke V2: <ports>. PASS end-to-end. Trace block was decisive evidence.

Tags retired: v2.0-f20.a.1-partial, v2.0-f20.a.1.b-partial.
Replaces memory files: project_f20a1_partial.md, project_f20a1b_partial.md, project_f20a1c_planned.md.
```

Then delete `project_f20a1_partial.md`, `project_f20a1b_partial.md`, `project_f20a1c_planned.md`, and update `MEMORY.md` (one line for the new done entry; remove the three stale lines).

---

## Self-Review Checklist (run before handing off to executor)

- ✅ **Spec coverage:** Every field listed in `project_f20a1c_planned.md` (s_last_tx_status, s_peripheral_active_at_handoff, s_extract_call_count, s_extract_entries_seen, s_extract_first_pdu_type, s_advertise_iterations) is added in Tasks 2–3 and parsed in Task 4.
- ✅ **No placeholders:** all code blocks contain real C / Python / shell.
- ✅ **Type consistency:** field names match across firmware (`extractCallCount`), Python dataclass (`extract_call_count`), and tests (`extract_call_count`).
- ✅ **Wire format:** documented in the layout table; matches the hand-rolled offsets in the firmware buffer (Task 3) and the Python parser (Task 4).
- ⚠ **Off-by-one watch:** the new `count` byte sits at offset 33; `entries[i]` base is `34 + i*17`. Both firmware (Task 3 Step 3) and Python (Task 4 Step 2) agree.
- ⚠ **Sentinel value:** `extract_first_pdu_type` is initialized to `0xFF` per parser call so "no entries seen" is distinguishable from "ADV_IND (0x00)". Test 5 covers this round-trip.

