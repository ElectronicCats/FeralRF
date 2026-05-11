# F20.a.1.e — HW RX Counters Diagnostic Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose the per-iteration HW counters from `rfc_bleAdvOutput_t` (accumulated across the F21 ADV loop) so the next smoke run definitively answers: does the slave's radio see the master's CONNECT_IND at all, and if yes, does HW filter accept or ignore it?

**Architecture:** Replaces hypothesis-guessing with hardware-truth data. After F20.a.1.d ruled out AdvA mismatch and showed uniform BLE_DONE_NOSYNC, the remaining question is whether the slave radio HW even *sees* the master's CONNECT_IND in its RX window. `rfc_bleAdvOutput_t` answers this via four counters (`nTxAdvInd`, `nRxConnectReq`, `nRxIgnored`, `nRxNok`) plus `lastRssi`. After this plan lands, one smoke run identifies the failure mode and the actual fix follows in F20.a.1.f.

**Tech Stack:** TI SimpleLink CC13xx/CC26xx SDK 8.30 (`rfc_bleAdvOutput_t`), Python 3.12, pytest, COBS framing.

**Branch:** Continue on `feature/f20a1-peripheral-read` (HEAD `2f7e397`, tag `v2.0-f20.a.1.d-partial`).

---

## File Structure

| File | Change | Purpose |
|------|--------|---------|
| `firmware/cc1352/src/radio_if.c` | Modify | Add 5 accumulator statics + sum per-iter from `s_f21_adv_output`; update getter |
| `firmware/cc1352/include/radio_if.h` | Modify | Add fields to `RadioIF_DbgF21Trace` |
| `firmware/cc1352/src/command_processor.c` | Modify | Wire layout 42 B → 51 B header, DEPTH 12 → 11 (51 + 11*17 = 238 ≤ 255) |
| `firmware/cc1352/include/ble_conn_mgr.h` | Modify | DEPTH constant 12 → 11, comment update |
| `python/feralrf/radio.py` | Modify | Add 5 fields to `SlaveDbgResult`, parse new wire layout |
| `python/tests/test_radio_debug_slave.py` | Modify | Update `_build_payload` + new tests TDD-style |
| `python/examples/smoke_f20a1_b_diag.py` | Modify | Print HW counters with branching diagnostic interpretation |

---

## Wire Layout Reference (after Task 3)

```
Offset  Size  Field
   0     4   accessAddr
   4     4   crcInit
   8     2   winOffset_125us
  10     2   hopInterval_125us
  12     2   latency
  14     2   supervTimeout_10ms
  16     1   hopIncrement
  17     4   connectIndEndRat
  21     4   firstAnchorRat
  25     2   f21LastStatus
  27     1   peripheralActiveAtHandoff
  28     1   extractCallCount
  29     1   extractEntriesSeen
  30     1   extractFirstPduType
  31     2   advertiseIterations
  33     2   f21FirstNonzeroStatus
  35     6   f21AdvA[6]
  --- F20.a.1.e HW counters (9 B) ---
  41     2   f21TotalTxAdvInd            u16 LE   [NEW, saturating]
  43     2   f21TotalRxConnectReq        u16 LE   [NEW, saturating]
  45     2   f21TotalRxIgnored           u16 LE   [NEW, saturating]
  47     2   f21TotalRxNok               u16 LE   [NEW, saturating]
  49     1   f21LastRssi                 i8       [NEW]
  ---
  50     1   count (n)                   u8       [moved from off 41]
  51   n*17  entries[n]
```

Total at DEPTH 11: `51 + 11*17 = 238 B ≤ 255`. DEPTH dropped from 12 to 11 to fit; smoke V2 only inspects the first ~3 entries anyway, so losing 1 slot is harmless.

---

## Task 1: Add HW counter accumulators in radio_if.c

**Files:**
- Modify: `firmware/cc1352/src/radio_if.c:170-185` (state declarations)
- Modify: `firmware/cc1352/src/radio_if.c:686-828` (`RadioIF_transmitBleAdvLegacy`)

- [ ] **Step 1: Add 5 new statics + saturating helper macro**

After the existing F20.a.1.d statics (around line 180), add:

```c
/* F20.a.1.e — HW counters from rfc_bleAdvOutput_t accumulated across the F21
 * ADV loop. Each iteration's s_f21_adv_output is reset by memset before
 * RF_runCmd; we add its values to these accumulators *before* the next iter.
 * All counters saturate at U16_MAX so a stuck radio doesn't roll over and
 * mask a real problem. */
static uint16_t s_dbg_f21_total_tx_adv_ind = 0u;
static uint16_t s_dbg_f21_total_rx_connect_req = 0u;
static uint16_t s_dbg_f21_total_rx_ignored = 0u;
static uint16_t s_dbg_f21_total_rx_nok = 0u;
static int8_t s_dbg_f21_last_rssi = 0;
```

- [ ] **Step 2: Add saturating-add helper near the existing helpers in radio_if.c**

Search for a good location (top of file after other static helpers):

```bash
grep -n "^static.*Sat\|static inline" firmware/cc1352/src/radio_if.c | head -5
```

Add a static inline helper at file scope (placement: just before `static RF_Mode *RadioIF_getPropMode(void)` at line ~179, or just after the new statics above):

```c
static inline uint16_t RadioIF_satAddU16(uint16_t a, uint16_t b) {
    uint32_t sum = (uint32_t)a + (uint32_t)b;
    return (sum > 0xFFFFu) ? 0xFFFFu : (uint16_t)sum;
}
```

- [ ] **Step 3: Reset accumulators at the start of each `RadioIF_transmitBleAdvLegacy` call**

Inside `RadioIF_transmitBleAdvLegacy`, find the F20.a.1.d reset block (around line 714-718, immediately after `memcpy(s_dbg_f21_advA, addr, ...)`). Add the new resets right after:

```c
    /* F20.a.1.e — reset HW counter accumulators per call. */
    s_dbg_f21_total_tx_adv_ind = 0u;
    s_dbg_f21_total_rx_connect_req = 0u;
    s_dbg_f21_total_rx_ignored = 0u;
    s_dbg_f21_total_rx_nok = 0u;
    s_dbg_f21_last_rssi = 0;
```

- [ ] **Step 4: Accumulate counters after each iteration**

Inside the ADV loop in `RadioIF_transmitBleAdvLegacy` (around line 815-840), find the section right after `RadioIF_executeTxCommand` returns and BEFORE the `BLE_DONE_CONNECT` break check. The F20.a.1.d code there is:

```c
        if (!ok) {
            return false;
        }
        s_dbg_f21_last_status = cmd->status;
        if (s_dbg_f21_first_nonzero_status == 0u && cmd->status != BLE_DONE_OK) {
            s_dbg_f21_first_nonzero_status = cmd->status;
        }
        if (cmd->status == 0x1404u || cmd->status == 0x140Au) {
            break;
        }
```

Insert the accumulation BETWEEN the `s_dbg_f21_last_status = cmd->status;` line and the `if (s_dbg_f21_first_nonzero_status...)` line:

```c
        s_dbg_f21_last_status = cmd->status;
        /* F20.a.1.e — accumulate HW counters from this iteration's output.
         * s_f21_adv_output is memset'd to zero before each RF_runCmd above,
         * so its fields reflect only this iteration. */
        s_dbg_f21_total_tx_adv_ind =
            RadioIF_satAddU16(s_dbg_f21_total_tx_adv_ind, s_f21_adv_output.nTxAdvInd);
        s_dbg_f21_total_rx_connect_req =
            RadioIF_satAddU16(s_dbg_f21_total_rx_connect_req, s_f21_adv_output.nRxConnectReq);
        s_dbg_f21_total_rx_ignored =
            RadioIF_satAddU16(s_dbg_f21_total_rx_ignored, s_f21_adv_output.nRxIgnored);
        s_dbg_f21_total_rx_nok =
            RadioIF_satAddU16(s_dbg_f21_total_rx_nok, s_f21_adv_output.nRxNok);
        if (s_f21_adv_output.nRxScanReq > 0u || s_f21_adv_output.nRxConnectReq > 0u ||
            s_f21_adv_output.nRxIgnored > 0u) {
            s_dbg_f21_last_rssi = s_f21_adv_output.lastRssi;
        }
        if (s_dbg_f21_first_nonzero_status == 0u && cmd->status != BLE_DONE_OK) {
            s_dbg_f21_first_nonzero_status = cmd->status;
        }
```

- [ ] **Step 5: Build firmware to verify compile**

```bash
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -10
```

Expected: `[100%] Built target feralrf_cc1352`. No new warnings.

- [ ] **Step 6: Commit**

```bash
git add firmware/cc1352/src/radio_if.c
git commit -m "feat(f20.a.1.e): accumulate rfc_bleAdvOutput HW counters across F21 loop"
```

---

## Task 2: Expose HW counters via RadioIF_DbgF21Trace

**Files:**
- Modify: `firmware/cc1352/include/radio_if.h:200-220`
- Modify: `firmware/cc1352/src/radio_if.c:3125-3150` (`RadioIF_getDbgF21Trace`)

- [ ] **Step 1: Update the struct in radio_if.h**

Find the current `RadioIF_DbgF21Trace` struct (search `grep -n "RadioIF_DbgF21Trace" firmware/cc1352/include/radio_if.h`). Replace with:

```c
typedef struct {
    /* F20.a.1.d core trace */
    uint16_t f21LastStatus;
    uint16_t f21FirstNonzeroStatus;
    uint16_t advertiseIterations;
    uint8_t extractCallCount;
    uint8_t extractEntriesSeen;
    uint8_t extractFirstPduType;
    uint8_t f21AdvA[6];
    /* F20.a.1.e — HW counters accumulated across the F21 ADV loop.
     * f21LastRssi is the RSSI of the most recent RX'd packet (any kind).
     * Use these to disambiguate NOSYNC: nRxConnectReq>0 means radio HW saw
     * a valid CONNECT_IND; nRxIgnored>0 means a packet arrived but HW filter
     * rejected it; both 0 + nTxAdvInd > 0 means no packet ever entered the
     * RX window (timing miss). */
    uint16_t f21TotalTxAdvInd;
    uint16_t f21TotalRxConnectReq;
    uint16_t f21TotalRxIgnored;
    uint16_t f21TotalRxNok;
    int8_t f21LastRssi;
} RadioIF_DbgF21Trace;
```

- [ ] **Step 2: Update the getter body in radio_if.c**

Find `RadioIF_getDbgF21Trace` (around line 3125). Replace the body with:

```c
void RadioIF_getDbgF21Trace(RadioIF_DbgF21Trace *out) {
    if (out == NULL) {
        return;
    }
    out->f21LastStatus = s_dbg_f21_last_status;
    out->f21FirstNonzeroStatus = s_dbg_f21_first_nonzero_status;
    out->advertiseIterations = s_dbg_advertise_iterations;
    out->extractCallCount = s_dbg_extract_call_count;
    out->extractEntriesSeen = s_dbg_extract_entries_seen;
    out->extractFirstPduType = s_dbg_extract_first_pdu_type;
    memcpy(out->f21AdvA, s_dbg_f21_advA, sizeof(out->f21AdvA));
    out->f21TotalTxAdvInd = s_dbg_f21_total_tx_adv_ind;
    out->f21TotalRxConnectReq = s_dbg_f21_total_rx_connect_req;
    out->f21TotalRxIgnored = s_dbg_f21_total_rx_ignored;
    out->f21TotalRxNok = s_dbg_f21_total_rx_nok;
    out->f21LastRssi = s_dbg_f21_last_rssi;
}
```

- [ ] **Step 3: Build firmware**

```bash
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build. command_processor.c will not yet have callers for the new fields — that lands in Task 3. No errors expected since the new struct fields don't break old callers.

- [ ] **Step 4: Commit**

```bash
git add firmware/cc1352/include/radio_if.h firmware/cc1352/src/radio_if.c
git commit -m "feat(f20.a.1.e): expose HW counters + lastRssi via DbgF21Trace"
```

---

## Task 3: Update RSP_DEBUG_SLAVE wire layout 42 → 51 B header, DEPTH 12 → 11

**Files:**
- Modify: `firmware/cc1352/include/ble_conn_mgr.h` (DEPTH constant + comment)
- Modify: `firmware/cc1352/src/command_processor.c:1153-1280` (CMD_DEBUG_SLAVE handler)

- [ ] **Step 1: Update DEPTH constant in ble_conn_mgr.h**

```bash
grep -n "BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH" firmware/cc1352/include/ble_conn_mgr.h
```

Change line 84 from `12u` to `11u`. Also update the comment on line 82:

```c
 * Depth 11: 51 B header + 11*17 B = 238 B ≤ PROTOCOL_MAX_PAYLOAD (F20.a.1.e)
```

```c
#define BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH 11u
```

- [ ] **Step 2: Rewrite the layout comment in command_processor.c**

Find the F20.a.1.d layout comment in the CMD_DEBUG_SLAVE handler (search `grep -n "Wire layout (42 B header" firmware/cc1352/src/command_processor.c`). Replace with:

```c
        /* Wire layout (51 B header + n * 17 B entries) — F20.a.1.e:
         *   accessAddr               u32 LE   (4)   off  0
         *   crcInit                  u32 LE   (4)   off  4
         *   winOffset_125us          u16 LE   (2)   off  8
         *   hopInterval_125us        u16 LE   (2)   off 10
         *   latency                  u16 LE   (2)   off 12
         *   supervTimeout_10ms       u16 LE   (2)   off 14
         *   hopIncrement             u8       (1)   off 16
         *   connectIndEndRat         u32 LE   (4)   off 17
         *   firstAnchorRat           u32 LE   (4)   off 21
         *   --- F20.a.1.c trace block (8 B) ---
         *   f21LastStatus            u16 LE   (2)   off 25
         *   peripheralActiveAtHand   u8       (1)   off 27
         *   extractCallCount         u8       (1)   off 28
         *   extractEntriesSeen       u8       (1)   off 29
         *   extractFirstPduType      u8       (1)   off 30
         *   advertiseIterations      u16 LE   (2)   off 31
         *   --- F20.a.1.d trace block (8 B) ---
         *   f21FirstNonzeroStatus    u16 LE   (2)   off 33
         *   f21AdvA[6]               u8 LE    (6)   off 35
         *   --- F20.a.1.e HW counters (9 B) ---
         *   f21TotalTxAdvInd         u16 LE   (2)   off 41  [NEW]
         *   f21TotalRxConnectReq     u16 LE   (2)   off 43  [NEW]
         *   f21TotalRxIgnored        u16 LE   (2)   off 45  [NEW]
         *   f21TotalRxNok            u16 LE   (2)   off 47  [NEW]
         *   f21LastRssi              i8       (1)   off 49  [NEW]
         *   ---
         *   count                    u8       (1)   off 50  [moved from off 41]
         *   --- entries[n] start at off 51, 17 B each ---
         *     event_counter   u16 LE   (2)
         *     chan            u8       (1)
         *     anchor_rat      u32 LE   (4)
         *     actual_start    u32 LE   (4)
         *     status          u16 LE   (2)
         *     nRxOk           u8       (1)
         *     nRxNok          u8       (1)
         *     nRxIgnored      u8       (1)
         *     pktStatus       u8       (1)
         * Total at DEPTH 11: 51 + 11*17 = 238 B ≤ 255. */
```

- [ ] **Step 3: Update the rsp[] buffer size and packing**

Find the line `uint8_t rsp[42u + BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH * 17u];`. Change to:

```c
        uint8_t rsp[51u + BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH * 17u];
```

Find the existing trace block packing (`rsp[33-34]` for f21FirstNonzeroStatus, `memcpy(&rsp[35]` for f21AdvA, `rsp[41] = n`). REPLACE `rsp[41] = n;` with the new 9 B HW counter block + moved count:

```c
        /* F20.a.1.e HW counters block at offsets 41-49 */
        rsp[41] = (uint8_t)(trace.f21TotalTxAdvInd & 0xFFu);
        rsp[42] = (uint8_t)((trace.f21TotalTxAdvInd >> 8) & 0xFFu);
        rsp[43] = (uint8_t)(trace.f21TotalRxConnectReq & 0xFFu);
        rsp[44] = (uint8_t)((trace.f21TotalRxConnectReq >> 8) & 0xFFu);
        rsp[45] = (uint8_t)(trace.f21TotalRxIgnored & 0xFFu);
        rsp[46] = (uint8_t)((trace.f21TotalRxIgnored >> 8) & 0xFFu);
        rsp[47] = (uint8_t)(trace.f21TotalRxNok & 0xFFu);
        rsp[48] = (uint8_t)((trace.f21TotalRxNok >> 8) & 0xFFu);
        rsp[49] = (uint8_t)trace.f21LastRssi;  /* i8 stored as bit-pattern */
        rsp[50] = n;
```

- [ ] **Step 4: Update entries loop offset**

Find `&rsp[42u + (uint16_t)i * 17u]` and change `42u` to `51u`:

```c
        for (uint8_t i = 0u; i < n; i++) {
            uint8_t *p = &rsp[51u + (uint16_t)i * 17u];
```

- [ ] **Step 5: Update send_response length**

Find `send_response(seq, RSP_DEBUG_SLAVE, rsp, 42u + (uint16_t)n * 17u);`. Change `42u` to `51u`:

```c
        send_response(seq, RSP_DEBUG_SLAVE, rsp, 51u + (uint16_t)n * 17u);
```

- [ ] **Step 6: Build firmware**

```bash
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -10
```

Expected: clean build. Total response 238 B with DEPTH 11.

- [ ] **Step 7: Commit**

```bash
git add firmware/cc1352/include/ble_conn_mgr.h firmware/cc1352/src/command_processor.c
git commit -m "feat(f20.a.1.e): RSP_DEBUG_SLAVE 42→51 B header (HW counters), DEPTH 12→11"
```

---

## Task 4: Update Python SlaveDbgResult + parser (TDD)

**Files:**
- Modify: `python/tests/test_radio_debug_slave.py` (helper + tests first)
- Modify: `python/feralrf/radio.py:243-280` (dataclass) + `978-1050` (parser)

- [ ] **Step 1: Update `_build_payload` helper for 51 B layout**

In `python/tests/test_radio_debug_slave.py`, find `_build_payload` (around line 53). The F20.a.1.d helper currently packs 42 B and the `len(entries)` byte at offset 41. Replace the block from where `buf.extend(snapshot.get("f21_adv_a", ...))` lands through the count append with:

```python
    buf.extend(snapshot.get("f21_adv_a", b"\x00" * 6))                          # off 35
    # F20.a.1.e HW counters block (9 B)
    buf.extend(snapshot.get("f21_total_tx_adv_ind", 0).to_bytes(2, "little"))   # off 41
    buf.extend(snapshot.get("f21_total_rx_connect_req", 0).to_bytes(2, "little"))  # off 43
    buf.extend(snapshot.get("f21_total_rx_ignored", 0).to_bytes(2, "little"))   # off 45
    buf.extend(snapshot.get("f21_total_rx_nok", 0).to_bytes(2, "little"))       # off 47
    buf.append(snapshot.get("f21_last_rssi", 0) & 0xFF)                         # off 49 (i8 as unsigned byte)
    buf.append(len(entries))                                                    # off 50
```

Verify there is no leftover `buf.append(len(entries))` from the F20.a.1.d layout (only ONE such call should remain, at the new offset 50).

- [ ] **Step 2: Update `test_count_truncated_by_payload` count offset**

Find the line in `test_count_truncated_by_payload` that sets `full[41] = 5` (F20.a.1.d's count position). Change to `full[50] = 5`:

```bash
grep -n "full\[41\]\|full\[50\]" python/tests/test_radio_debug_slave.py
```

Replace:

```python
        full[50] = 5  # count moved to off 50 in F20.a.1.e
```

- [ ] **Step 3: Add 3 new tests for the HW counter fields**

At the end of `TestDebugSlaveParser` (after `test_f21_adv_a_round_trip`), add three tests. Each uses the same fixture-builder pattern as the existing F20.a.1.d tests:

```python
    def test_f21_hw_counters_round_trip(self):
        """F20.a.1.e — parser exposes the 4 HW counters (off 41-48)."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x0,
            "crc_init": 0x0,
            "win_offset": 0,
            "hop_interval": 0,
            "latency": 0,
            "superv_timeout": 0,
            "hop_increment": 0,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_total_tx_adv_ind": 200,
            "f21_total_rx_connect_req": 0,
            "f21_total_rx_ignored": 3,
            "f21_total_rx_nok": 1,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_total_tx_adv_ind == 200
        assert result.f21_total_rx_connect_req == 0
        assert result.f21_total_rx_ignored == 3
        assert result.f21_total_rx_nok == 1

    def test_f21_last_rssi_negative(self):
        """F20.a.1.e — parser decodes f21_last_rssi as signed int8 (off 49)."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x0,
            "crc_init": 0x0,
            "win_offset": 0,
            "hop_interval": 0,
            "latency": 0,
            "superv_timeout": 0,
            "hop_increment": 0,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_last_rssi": -47,  # typical BLE RSSI value
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_last_rssi == -47

    def test_f21_hw_counters_saturate(self):
        """F20.a.1.e — counters cap at 0xFFFF (saturating, not wrapping)."""
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x0,
            "crc_init": 0x0,
            "win_offset": 0,
            "hop_interval": 0,
            "latency": 0,
            "superv_timeout": 0,
            "hop_increment": 0,
            "connect_ind_end_rat": 0,
            "first_anchor_rat": 0,
            "f21_total_tx_adv_ind": 0xFFFF,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_total_tx_adv_ind == 0xFFFF
```

- [ ] **Step 4: Run tests, expect failures**

```bash
cd python && source .venv/bin/activate && pytest tests/test_radio_debug_slave.py -v 2>&1 | tail -40
```

Expected: the 3 new tests FAIL with `AttributeError`. Existing tests may also fail on the offset shift (count from 41 to 50).

- [ ] **Step 5: Update `SlaveDbgResult` dataclass**

In `python/feralrf/radio.py`, find the dataclass declaration (around line 243). Add 5 new fields after `f21_adv_a: bytes`:

```python
    f21_last_status: int
    peripheral_active_at_handoff: int
    extract_call_count: int
    extract_entries_seen: int
    extract_first_pdu_type: int
    advertise_iterations: int
    f21_first_nonzero_status: int
    f21_adv_a: bytes
    # F20.a.1.e — HW counters from rfc_bleAdvOutput_t, accumulated across the F21 loop
    f21_total_tx_adv_ind: int
    f21_total_rx_connect_req: int
    f21_total_rx_ignored: int
    f21_total_rx_nok: int
    f21_last_rssi: int  # signed int8 in [-128, 127]
    entries: tuple
```

Verify the existing field order before/after with `sed -n '243,290p' python/feralrf/radio.py` to make sure no other fields are after `entries`.

- [ ] **Step 6: Update the parser**

In `python/feralrf/radio.py`, find `debug_slave()` (around line 978). The F20.a.1.d parser reads `n = payload[41]` and entries from `42`. Update for the new layout:

```python
        if len(payload) < 51:
            raise ProtocolError(
                f"DEBUG_SLAVE payload too short: {len(payload)} < 51"
            )
        # ... existing reads up through f21_adv_a ...
        f21_adv_a = bytes(payload[35:41])
        # F20.a.1.e HW counters at offsets 41-49
        f21_total_tx_adv_ind = int.from_bytes(payload[41:43], "little")
        f21_total_rx_connect_req = int.from_bytes(payload[43:45], "little")
        f21_total_rx_ignored = int.from_bytes(payload[45:47], "little")
        f21_total_rx_nok = int.from_bytes(payload[47:49], "little")
        f21_last_rssi = int.from_bytes(payload[49:50], "little", signed=True)
        n = payload[50]
        entries_offset = 51
```

Update the `SlaveDbgResult(...)` constructor call to pass the 5 new fields:

```python
        return SlaveDbgResult(
            # ... existing args ...
            f21_adv_a=f21_adv_a,
            f21_total_tx_adv_ind=f21_total_tx_adv_ind,
            f21_total_rx_connect_req=f21_total_rx_connect_req,
            f21_total_rx_ignored=f21_total_rx_ignored,
            f21_total_rx_nok=f21_total_rx_nok,
            f21_last_rssi=f21_last_rssi,
            entries=tuple(entries),
        )
```

- [ ] **Step 7: Run tests, expect pass**

```bash
cd python && pytest tests/test_radio_debug_slave.py -v 2>&1 | tail -30
```

Expected: all tests in that file pass, including the 3 new ones.

- [ ] **Step 8: Full test suite**

```bash
cd python && pytest 2>&1 | tail -10
```

Expected: 596 passed (593 prior + 3 new), 1 skipped, 0 failed.

- [ ] **Step 9: Commit**

```bash
git add python/feralrf/radio.py python/tests/test_radio_debug_slave.py
git commit -m "feat(f20.a.1.e): SlaveDbgResult HW counters + RSSI parser"
```

---

## Task 5: Update smoke V2 to display HW counters with diagnostic interpretation

**Files:**
- Modify: `python/examples/smoke_f20a1_b_diag.py` (`trace_table` helper)

- [ ] **Step 1: Locate trace_table**

```bash
grep -n "def trace_table\|f21_adv_a" python/examples/smoke_f20a1_b_diag.py
```

- [ ] **Step 2: Add 5 new rows + diagnostic verdict block**

After the existing `f21_adv_a` row, add 5 rows for the new fields:

```python
    lines.append(
        f"f21_total_tx_adv_ind           {result.f21_total_tx_adv_ind:<17d}HW count of ADV_IND packets fully TX'd"
    )
    lines.append(
        f"f21_total_rx_connect_req       {result.f21_total_rx_connect_req:<17d}HW count of CONNECT_IND accepted by radio filter"
    )
    lines.append(
        f"f21_total_rx_ignored           {result.f21_total_rx_ignored:<17d}HW count of packets RX'd OK but ignored (filter mismatch)"
    )
    lines.append(
        f"f21_total_rx_nok               {result.f21_total_rx_nok:<17d}HW count of CRC-error packets in RX window"
    )
    lines.append(
        f"f21_last_rssi                  {result.f21_last_rssi:<17d}dBm of last RX'd packet (any kind)"
    )
```

Then add a NEW diagnostic helper function near `_interpret_ble_status` (place above `trace_table`):

```python
def _diagnose_nosync(result) -> str:
    """F20.a.1.e — interpret HW counters to identify root cause of NOSYNC.

    Returns a single-line verdict based on which counters are non-zero.
    """
    tx = result.f21_total_tx_adv_ind
    cr = result.f21_total_rx_connect_req
    ig = result.f21_total_rx_ignored
    nok = result.f21_total_rx_nok

    if tx == 0:
        return "SLAVE NEVER TX'd ADV_IND — RF setup failed before loop entry"
    if cr > 0:
        return (
            f"CONNECT_IND was accepted by HW filter ({cr}x) — bug is in TI status "
            "code interpretation or BLE state machine. NOT a radio-layer reject."
        )
    if ig > 0:
        return (
            f"CONNECT_IND or other packets arrived ({ig}x) but HW filter rejected "
            "them — check AdvA/InitA address bytes at radio level; LSB-first wire "
            "encoding might be inverted vs. firmware-side stored bytes."
        )
    if nok > 0:
        return (
            f"Packets arrived in RX window but {nok}x had CRC errors — RF/antenna "
            "issue or different access address (0x8E89BED6 expected on adv chan)."
        )
    return (
        "NO packets arrived in RX window across the entire loop — CONNECT_IND "
        "never lands inside the T_IFS=150µs window after slave's ADV_IND TX. "
        "Master timing too slow / master scanning other channels / master not "
        "actually attempting connection."
    )
```

After the last `lines.append` for `f21_last_rssi`, add a separator + verdict line:

```python
    lines.append("")
    lines.append(f"NOSYNC verdict: {_diagnose_nosync(result)}")
```

- [ ] **Step 3: Syntax check**

```bash
python -c "import ast; ast.parse(open('python/examples/smoke_f20a1_b_diag.py').read())"
```

Expected: no output.

- [ ] **Step 4: Pre-commit**

```bash
pre-commit run --files python/examples/smoke_f20a1_b_diag.py 2>&1 | tail -10
```

If any auto-fix, accept (re-stage).

- [ ] **Step 5: Commit**

```bash
git add python/examples/smoke_f20a1_b_diag.py
git commit -m "test(f20.a.1.e): smoke V2 prints HW counters + NOSYNC verdict"
```

---

## Task 6: Clean build + full test suite

- [ ] **Step 1: Clean firmware build**

```bash
cd firmware/cc1352 && rm -rf build && mkdir build && cd build
cmake .. 2>&1 | tail -5
make -j$(nproc) 2>&1 | tail -10
```

Expected: `[100%] Built target feralrf_cc1352`.

- [ ] **Step 2: Record hex hash**

```bash
ls -lh firmware/cc1352/build/feralrf_cc1352.hex
sha256sum firmware/cc1352/build/feralrf_cc1352.hex
```

- [ ] **Step 3: Full Python test suite**

```bash
cd python && pytest 2>&1 | tail -10
```

Expected: 596 passed, 1 skipped, 0 failed.

- [ ] **Step 4: Pre-commit final sweep**

```bash
pre-commit run --files \
  firmware/cc1352/src/radio_if.c \
  firmware/cc1352/include/radio_if.h \
  firmware/cc1352/src/command_processor.c \
  firmware/cc1352/include/ble_conn_mgr.h \
  python/feralrf/radio.py \
  python/tests/test_radio_debug_slave.py \
  python/examples/smoke_f20a1_b_diag.py 2>&1 | tail -15
```

Expected: all pass.

- [ ] **Step 5: No commit needed** (verification only)

---

## Task 7: Flash both boards + run smoke + interpret

This task requires the user's hardware. Inline execution, not subagent.

- [ ] **Step 1: Detect ports**

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip devices
```

Note the ACM port assignments. Ports rotate per session.

- [ ] **Step 2: Flash master (board #1)**

```bash
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

Retry once if first attempt fails (`feedback_flash_retry`).

- [ ] **Step 3: Flash slave (board #2)**

```bash
python -m catnip flash -d 2 /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

Retry once if first attempt fails.

- [ ] **Step 4: Run smoke V2 with count=200**

Use the actual port assignments from Step 1. Example assuming master on ACM3, slave on ACM0:

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
source .venv/bin/activate
python examples/smoke_f20a1_b_diag.py --peripheral-port /dev/ttyACM0 --central-port /dev/ttyACM3 --peripheral-count 200 \
  > docs/superpowers/evidence/2026-05-11-f20a1e-hw-counters-200.txt 2>&1
tail -40 docs/superpowers/evidence/2026-05-11-f20a1e-hw-counters-200.txt
```

- [ ] **Step 5: Interpret the verdict line**

Read the `NOSYNC verdict:` line from the smoke output. Cross-reference with the `_diagnose_nosync` matrix:

| Counter pattern | Verdict | Next action |
|-----------------|---------|-------------|
| `tx==0` | Slave never TX'd | Check RF setup in `RadioIF_transmitBleAdvLegacy` startup path. Investigate Ble5_0_mode init. |
| `cr > 0` | CONNECT_IND accepted | Bug is in TI BLE status interpretation. Investigate `BLE_DONE_*` enum values vs. observed status. |
| `ig > 0` | CONNECT_IND ignored | HW filter rejected. Check AdvA byte order — Python `_mac_str_to_le_bytes` vs firmware `memcpy(s_f21_device_addr, addr, 6)` vs TI `pDeviceAddress`. Endianness of `peerAddrType` bit. |
| `nok > 0` | CRC errors | Verify whitening init = `0x40 + 37` matches on master vs slave. Verify same PHY (1M). |
| All 0 + `tx > 0` | Timing miss | Master takes > T_IFS=150µs after slave's ADV_IND. Investigate `connectTime` offset, master scan window timing, or BLE5 extended-adv vs legacy timing. |

- [ ] **Step 6: Commit smoke output as evidence**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git add docs/superpowers/evidence/2026-05-11-f20a1e-hw-counters-200.txt
git commit -m "test(f20.a.1.e): HW counters smoke — NOSYNC verdict captured"
```

- [ ] **Step 7: Document the verdict + next-vuelta scope**

Write `docs/superpowers/evidence/2026-05-11-f20a1e-verdict.md` (~30 lines): summarize the counter values, the verdict, and what F20.a.1.f should target. Commit it.

---

## Task 8: Tag + branch decision

- [ ] **Step 1: Tag**

```bash
git tag v2.0-f20.a.1.e-partial
git push origin feature/f20a1-peripheral-read --tags
```

- [ ] **Step 2: Update memory**

Write `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f20a1e_partial.md` with the verdict + F20.a.1.f scope hint. Update `MEMORY.md` index with a one-line entry.

- [ ] **Step 3: Decide next action with user**

Possible paths after Task 7's verdict:
- **Tiny fix** (1-3 LOC): inline in this session as `feat(f20.a.1.e): fix <verdict>` then re-smoke to validate close.
- **Medium fix** (~10-50 LOC): write F20.a.1.f plan via `superpowers:writing-plans`.
- **Architectural pivot** (switch to `CMD_BLE5_ADV_LEGACY` or similar): F20.a.1.f or higher; needs investigation of TI multi_protocol patch compatibility (per `radio_if.c:577` note).

---

## Self-Review

- **Spec coverage:** Every counter in `rfc_bleAdvOutput_t` worth capturing (nTxAdvInd, nRxConnectReq, nRxIgnored, nRxNok, lastRssi) has a task that wires it from firmware through Python to smoke output. The verdict matrix in Task 7 Step 5 names the next-step direction for each possible counter pattern.
- **Placeholder scan:** All "TBD"-style language replaced with concrete code or grep commands.
- **Type consistency:** `f21TotalTxAdvInd` (C) ↔ `f21_total_tx_adv_ind` (Python) consistent. Wire offsets 41/43/45/47/49 documented identically in firmware comment + parser + test helper + struct layout reference at top of plan.
- **Ambiguity check:** Saturating add behavior on u16 explicitly noted (RadioIF_satAddU16 helper). i8 RSSI handling explicit in parser (`signed=True`).

## Out of Scope

- The actual fix for whichever failure mode the verdict identifies. That is F20.a.1.f.
- Switching CMD_BLE_ADV to a BLE5 variant — premature without the verdict.
- Sniffle wire capture — still unavailable (user has 2 boards only).
- Address randomization, SMP, pairing — separate phases.

## Done Criteria

- All 596 tests pass.
- Smoke V2 prints 5 new HW counter rows + a one-line verdict.
- Smoke output committed to evidence dir.
- Tag `v2.0-f20.a.1.e-partial` pushed.
- A clear next-vuelta direction (F20.a.1.f scope) documented.
