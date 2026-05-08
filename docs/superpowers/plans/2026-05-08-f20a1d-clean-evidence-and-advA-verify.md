# F20.a.1.d — Clean Evidence and AdvA Wire Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the contaminated `s_last_tx_status` reading in the F20.a.1.c trace with dedicated F21-only counters, capture the AdvA actually used by `CMD_BLE_ADV`, then run a Sniffle dual-capture to confirm or rule out AdvA mismatch as the NOSYNC root cause.

**Architecture:** Three sequential phases enforced by the systematic-debugging Iron Law (no fixes without root cause).
- **Phase 0 (Tasks 1–7):** Firmware adds `s_dbg_f21_last_status`, `s_dbg_f21_first_nonzero_status`, `s_dbg_f21_advA[6]`, all written ONLY from the F21 ADV loop. Wire layout in `RSP_DEBUG_SLAVE` grows from 34 B header to 42 B. Python parser + unit tests + smoke V2 updated to match.
- **Phase 1 (Tasks 8–10):** Build, flash, smoke V2 with Sniffle capturing both ADV_IND (slave→air ch37) and CONNECT_IND (master→air ch37). Compare AdvA bytes wire vs firmware report.
- **Phase 2 (Task 11):** Branch — if AdvA mismatch, fix in 1–2 lines and close F20.a.1; if AdvA matches, document evidence and write F20.a.1.e stub for the next hypothesis (ChSel#2 or timing).

**Tech Stack:** TI SimpleLink CC13xx/CC26xx SDK 8.30 (rfc_CMD_BLE_ADV legacy), Python 3.12, pytest, COBS framing, Sniffle (separate CC1352 board for wire capture).

**Branch:** Continue on `feature/f20a1-peripheral-read` (HEAD `44200dc`, tag `v2.0-f20.a.1.c-partial`). No worktree needed — incremental commits on top.

---

## File Structure

| File | Responsibility | Change |
|------|----------------|--------|
| `firmware/cc1352/src/radio_if.c` | F21 ADV loop + trace state | **Modify**: add 3 dedicated debug statics, write from `RadioIF_transmitBleAdvLegacy`, populate trace getter |
| `firmware/cc1352/include/radio_if.h` | `RadioIF_DbgF21Trace` struct | **Modify**: rename `lastTxStatus` → `f21LastStatus`; add `f21FirstNonzeroStatus`, `f21AdvA[6]` |
| `firmware/cc1352/src/command_processor.c` | `RSP_DEBUG_SLAVE` wire packing | **Modify**: 34 B → 42 B header, ring DEPTH stays 12 (246 B ≤ 255) |
| `python/feralrf/radio.py` | `SlaveDbgResult` + `debug_slave()` parser | **Modify**: rename field, add 2 new fields, parse 42 B header |
| `python/tests/test_radio_debug_slave.py` | Parser unit tests | **Modify**: regenerate fixtures for 42 B header |
| `python/examples/smoke_f20a1_b_diag.py` | Smoke V2 with trace pretty-print | **Modify**: print new fields with interpretive notes |

---

## Wire Layout Reference (after Task 3)

```
Offset  Size  Field
------  ----  -----
   0     4   accessAddr               u32 LE
   4     4   crcInit                  u32 LE
   8     2   winOffset_125us          u16 LE
  10     2   hopInterval_125us        u16 LE
  12     2   latency                  u16 LE
  14     2   supervTimeout_10ms       u16 LE
  16     1   hopIncrement             u8
  17     4   connectIndEndRat         u32 LE
  21     4   firstAnchorRat           u32 LE
  25     2   f21LastStatus            u16 LE   [RENAMED from lastTxStatus]
  27     1   peripheralActiveAtHand   u8
  28     1   extractCallCount         u8
  29     1   extractEntriesSeen       u8
  30     1   extractFirstPduType      u8
  31     2   advertiseIterations      u16 LE
  33     2   f21FirstNonzeroStatus    u16 LE   [NEW]
  35     6   f21AdvA[6]               u8 LE    [NEW]
  41     1   count (n)                u8       [moved from off 33]
  42    n*17 entries[n] (17 B each, layout unchanged)
```

Total at DEPTH 12: 42 + 12*17 = **246 B** ≤ 255 ✓.

---

## Task 1: Add dedicated F21 status statics in radio_if.c

**Files:**
- Modify: `firmware/cc1352/src/radio_if.c:170-178` (state declarations)
- Modify: `firmware/cc1352/src/radio_if.c:686-823` (`RadioIF_transmitBleAdvLegacy`)

- [ ] **Step 1: Declare new state variables**

After line 175 (`static uint16_t s_dbg_advertise_iterations = 0u;`), add:

```c
/* F20.a.1.d — dedicated F21 ADV trace, written ONLY from
 * RadioIF_transmitBleAdvLegacy. Replaces the polluted s_last_tx_status
 * read in the trace getter, which captured leakage from Prop433/cmd_test
 * paths instead of the actual CMD_BLE_ADV exit status. */
static uint16_t s_dbg_f21_last_status = 0u;
static uint16_t s_dbg_f21_first_nonzero_status = 0u;
static uint8_t s_dbg_f21_advA[6] = {0};
```

- [ ] **Step 2: Reset trace state on each call into the F21 ADV path**

In `RadioIF_transmitBleAdvLegacy`, immediately after `memcpy(s_f21_device_addr, addr, BLE_ADV_TX_DEVICE_ADDR_LEN);` (radio_if.c:706), insert:

```c
    /* F20.a.1.d — capture AdvA actually used and reset per-call counters
     * so each new advertise call has a clean evidence trail. */
    memcpy(s_dbg_f21_advA, addr, sizeof(s_dbg_f21_advA));
    s_dbg_f21_last_status = 0u;
    s_dbg_f21_first_nonzero_status = 0u;
```

- [ ] **Step 3: Capture cmd->status from each ADV iteration**

In the ADV loop at radio_if.c:799-822, after `cmd->status = 0x0000;` (line 805) and before the BLE_DONE_CONNECT check (line 812), the iteration runs `RadioIF_executeTxCommand` (lines 806-807). Replace lines 808-814 with:

```c
        if (!ok) {
            return false;
        }
        /* F20.a.1.d — record the actual exit status of CMD_BLE_ADV per
         * iteration. last_status reflects the most recent iteration;
         * first_nonzero_status pins the first iteration whose status was
         * not BLE_DONE_OK (helps pinpoint when CONNECT_IND attempts hit
         * a parser/RX-path failure mid-loop). */
        s_dbg_f21_last_status = cmd->status;
        if (s_dbg_f21_first_nonzero_status == 0u && cmd->status != 0x1400u) {
            s_dbg_f21_first_nonzero_status = cmd->status;
        }
        /* BLE_DONE_CONNECT = 0x1404, BLE_DONE_CONNECT_CHSEL0 = 0x140A. */
        if (cmd->status == 0x1404u || cmd->status == 0x140Au) {
            break;
        }
```

- [ ] **Step 4: Build firmware to verify compile**

```bash
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -20
```

Expected: `[100%] Built target feralrf_cc1352`. No new warnings on the added statics.

- [ ] **Step 5: Commit**

```bash
git add firmware/cc1352/src/radio_if.c
git commit -m "feat(f20.a.1.d): dedicated s_dbg_f21_last_status / first_nonzero / advA"
```

---

## Task 2: Expose new fields via RadioIF_DbgF21Trace

**Files:**
- Modify: `firmware/cc1352/include/radio_if.h:200-213`
- Modify: `firmware/cc1352/src/radio_if.c:3125-3134` (`RadioIF_getDbgF21Trace`)

- [ ] **Step 1: Update the struct in radio_if.h**

Replace the `RadioIF_DbgF21Trace` struct at radio_if.h:205-211 with:

```c
typedef struct {
    /* F20.a.1.d — f21LastStatus replaces the polluted lastTxStatus.
     * Written ONLY from RadioIF_transmitBleAdvLegacy's CMD_BLE_ADV loop.
     * f21FirstNonzeroStatus pins the first iteration whose status was
     * not BLE_DONE_OK (0x1400). f21AdvA[6] records the address bytes
     * actually used as pDeviceAddress for the most recent advertise call. */
    uint16_t f21LastStatus;
    uint16_t f21FirstNonzeroStatus;
    uint16_t advertiseIterations;
    uint8_t extractCallCount;
    uint8_t extractEntriesSeen;
    uint8_t extractFirstPduType;
    uint8_t f21AdvA[6];
} RadioIF_DbgF21Trace;
```

- [ ] **Step 2: Update the getter in radio_if.c**

Replace `RadioIF_getDbgF21Trace` body at radio_if.c:3125-3134 with:

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
}
```

- [ ] **Step 3: Build firmware to verify compile**

```bash
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -20
```

Expected: `[100%] Built target feralrf_cc1352`. May see warnings if any caller still references `lastTxStatus` (Task 3 will fix).

- [ ] **Step 4: Commit**

```bash
git add firmware/cc1352/include/radio_if.h firmware/cc1352/src/radio_if.c
git commit -m "feat(f20.a.1.d): expose f21-specific status + AdvA via DbgF21Trace"
```

---

## Task 3: Update RSP_DEBUG_SLAVE wire layout (34 B → 42 B header)

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c:1153-1260` (CMD_DEBUG_SLAVE handler)

- [ ] **Step 1: Rewrite the layout comment block**

Replace the comment at command_processor.c:1169-1196 with:

```c
        /* Wire layout (42 B header + n * 17 B entries) — F20.a.1.d:
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
         *   f21LastStatus            u16 LE   (2)   off 25  [renamed]
         *   peripheralActiveAtHand   u8       (1)   off 27
         *   extractCallCount         u8       (1)   off 28
         *   extractEntriesSeen       u8       (1)   off 29
         *   extractFirstPduType      u8       (1)   off 30
         *   advertiseIterations      u16 LE   (2)   off 31
         *   --- F20.a.1.d trace block (8 B) ---
         *   f21FirstNonzeroStatus    u16 LE   (2)   off 33  [new]
         *   f21AdvA[6]               u8 LE    (6)   off 35  [new]
         *   ---
         *   count                    u8       (1)   off 41
         *   --- entries[n] start at off 42, 17 B each ---
         *     event_counter   u16 LE   (2)
         *     chan            u8       (1)
         *     anchor_rat      u32 LE   (4)
         *     actual_start    u32 LE   (4)
         *     status          u16 LE   (2)
         *     nRxOk           u8       (1)
         *     nRxNok          u8       (1)
         *     nRxIgnored      u8       (1)
         *     pktStatus       u8       (1)
         * Total at DEPTH 12: 42 + 12*17 = 246 B ≤ 255. */
```

- [ ] **Step 2: Update the response buffer size and packing**

Replace command_processor.c:1197 (`uint8_t rsp[34u + ...]`) and the trace-block packing (lines 1223-1231) and the count assignment (line 1231) with:

```c
        uint8_t rsp[42u + BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH * 17u];
```

Then keep rsp[0]–rsp[24] unchanged (lines 1198-1222), and replace lines 1223-1231 with:

```c
        rsp[25] = (uint8_t)(trace.f21LastStatus & 0xFFu);
        rsp[26] = (uint8_t)((trace.f21LastStatus >> 8) & 0xFFu);
        rsp[27] = s_dbg_peripheral_active_at_handoff;
        rsp[28] = trace.extractCallCount;
        rsp[29] = trace.extractEntriesSeen;
        rsp[30] = trace.extractFirstPduType;
        rsp[31] = (uint8_t)(trace.advertiseIterations & 0xFFu);
        rsp[32] = (uint8_t)((trace.advertiseIterations >> 8) & 0xFFu);
        rsp[33] = (uint8_t)(trace.f21FirstNonzeroStatus & 0xFFu);
        rsp[34] = (uint8_t)((trace.f21FirstNonzeroStatus >> 8) & 0xFFu);
        memcpy(&rsp[35], trace.f21AdvA, 6u);
        rsp[41] = n;
```

- [ ] **Step 3: Update the entries loop offset**

The entries loop at command_processor.c:1233 starts entries at `&rsp[34u + ...]`. Change to `&rsp[42u + ...]`:

```c
        for (uint8_t i = 0u; i < n; i++) {
            uint8_t *p = &rsp[42u + (uint16_t)i * 17u];
```

- [ ] **Step 4: Update the send_response call**

Find the `send_response(seq, RSP_DEBUG_SLAVE, rsp, 34u + (uint16_t)n * 17u);` call (or similar) at the end of the CMD_DEBUG_SLAVE handler and change `34u` to `42u`.

```bash
grep -n "RSP_DEBUG_SLAVE.*rsp.*34u" firmware/cc1352/src/command_processor.c
```

Then replace the call with:

```c
        send_response(seq, RSP_DEBUG_SLAVE, rsp, 42u + (uint16_t)n * 17u);
```

- [ ] **Step 5: Build firmware**

```bash
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -30
```

Expected: clean build with `feralrf_cc1352.hex` regenerated. Verify file timestamp is fresh:

```bash
ls -lh firmware/cc1352/build/feralrf_cc1352.hex
```

- [ ] **Step 6: Commit**

```bash
git add firmware/cc1352/src/command_processor.c
git commit -m "feat(f20.a.1.d): RSP_DEBUG_SLAVE 34 B → 42 B header (f21FirstNonzero + AdvA)"
```

---

## Task 4: Update SlaveDbgResult dataclass in radio.py (TDD: tests first)

**Files:**
- Modify: `python/tests/test_radio_debug_slave.py` (regenerate fixtures)
- Modify: `python/feralrf/radio.py:243-280` (`SlaveDbgResult` dataclass)
- Modify: `python/feralrf/radio.py:978-1046` (`debug_slave` parser)

This task uses TDD: update tests first to match the new layout, watch them fail, then update the dataclass and parser.

- [ ] **Step 1: Update the `_build_payload` helper to the new 42 B layout**

The existing helper at `python/tests/test_radio_debug_slave.py:53-83` packs the F20.a.1.c 9 B trace block. Replace lines 65-72 (the trace-block region + count append) with the new 16 B trace block + count:

```python
    # F20.a.1.d trace block (16 B): renames last_tx_status → f21_last_status,
    # adds f21_first_nonzero_status (u16) + f21_adv_a (6 B). count moves to off 41.
    buf.extend(snapshot.get("f21_last_status", 0).to_bytes(2, "little"))         # off 25
    buf.append(snapshot.get("peripheral_active_at_handoff", 0))                  # off 27
    buf.append(snapshot.get("extract_call_count", 0))                            # off 28
    buf.append(snapshot.get("extract_entries_seen", 0))                          # off 29
    buf.append(snapshot.get("extract_first_pdu_type", 0))                        # off 30
    buf.extend(snapshot.get("advertise_iterations", 0).to_bytes(2, "little"))    # off 31
    buf.extend(snapshot.get("f21_first_nonzero_status", 0).to_bytes(2, "little"))  # off 33
    buf.extend(snapshot.get("f21_adv_a", b"\x00" * 6))                           # off 35
    buf.append(len(entries))                                                     # off 41
```

(The existing `buf.append(len(entries))` at line 72 is moved to off 41 — keep just the one append; ensure no duplicate.)

- [ ] **Step 2: Rename `last_tx_status` references in test bodies**

The current `test_trace_fields_round_trip` test at line 227 uses keys like `"last_tx_status"` and asserts `result.last_tx_status`. Rename throughout:

```bash
sed -i 's/"last_tx_status"/"f21_last_status"/g' python/tests/test_radio_debug_slave.py
sed -i 's/result\.last_tx_status/result.f21_last_status/g' python/tests/test_radio_debug_slave.py
```

Verify the changes:

```bash
grep -n "last_tx_status\|f21_last_status" python/tests/test_radio_debug_slave.py
```

- [ ] **Step 3: Add 2 new tests for the new fields**

At the end of the `TestDebugSlaveParser` class (after `test_trace_fields_round_trip`), add two new methods using the same `_radio_with_fake_serial` + `_build_payload` + `fake.queue_response` pattern as existing tests:

```python
    def test_f21_first_nonzero_status_round_trip(self):
        """F20.a.1.d — parser exposes f21_first_nonzero_status (u16 LE off 33)."""
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
            "f21_last_status": 0x1404,
            "f21_first_nonzero_status": 0x1402,  # BLE_DONE_NOSYNC mid-loop
            "advertise_iterations": 100,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_first_nonzero_status == 0x1402
        assert result.f21_last_status == 0x1404

    def test_f21_adv_a_round_trip(self):
        """F20.a.1.d — parser exposes the 6-byte AdvA used by CMD_BLE_ADV (off 35)."""
        radio, fake = _radio_with_fake_serial()
        expected_adv_a = bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])
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
            "f21_adv_a": expected_adv_a,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert result.f21_adv_a == expected_adv_a
```

- [ ] **Step 4: Run tests, expect failures**

```bash
cd python && source .venv/bin/activate && pytest tests/test_radio_debug_slave.py -v 2>&1 | tail -40
```

Expected: 2 new tests FAIL with `AttributeError: 'SlaveDbgResult' object has no attribute 'f21_first_nonzero_status'`. Existing tests may also FAIL or ERROR on the rename.

- [ ] **Step 5: Update SlaveDbgResult dataclass**

In `python/feralrf/radio.py`, find the `@dataclass` for `SlaveDbgResult` (around line 243). Rename the `last_tx_status: int` field to `f21_last_status: int` and add two new fields:

```python
@dataclass(frozen=True)
class SlaveDbgResult:
    # ... existing fields up to extract_first_pdu_type ...
    f21_last_status: int           # F20.a.1.d — was last_tx_status (renamed)
    peripheral_active_at_handoff: int
    extract_call_count: int
    extract_entries_seen: int
    extract_first_pdu_type: int
    advertise_iterations: int
    f21_first_nonzero_status: int  # F20.a.1.d — first non-OK iter status, 0 if none
    f21_adv_a: bytes               # F20.a.1.d — 6-byte AdvA used in last call
    entries: tuple
    # ... any remaining fields ...
```

Verify the field list against the current declaration:

```bash
sed -n '243,280p' python/feralrf/radio.py
```

- [ ] **Step 6: Update debug_slave() parser offsets**

In `python/feralrf/radio.py`, find the parser body starting at `def debug_slave` (around line 978). Update the offsets after the existing `advertise_iterations = int.from_bytes(payload[31:33], "little")` line:

```python
    last_tx_status = int.from_bytes(payload[25:27], "little")  # rename: f21_last_status
    # ... existing reads up through advertise_iterations ...
    advertise_iterations = int.from_bytes(payload[31:33], "little")
    f21_first_nonzero_status = int.from_bytes(payload[33:35], "little")
    f21_adv_a = bytes(payload[35:41])
    n = payload[41]
    # entries start at off 42 (was 34)
    entries_offset = 42
```

Update the variable name `last_tx_status` to `f21_last_status` everywhere in the function, and update the `SlaveDbgResult(...)` constructor call to pass the new fields:

```python
    return SlaveDbgResult(
        # ... existing args up through extract_first_pdu_type ...
        f21_last_status=last_tx_status,   # variable rename
        # ...
        advertise_iterations=advertise_iterations,
        f21_first_nonzero_status=f21_first_nonzero_status,
        f21_adv_a=f21_adv_a,
        entries=tuple(entries),
    )
```

Search for any remaining `last_tx_status` in radio.py and rename:

```bash
grep -n "last_tx_status" python/feralrf/radio.py
```

If any matches, rename to `f21_last_status`.

- [ ] **Step 7: Run tests, expect pass**

```bash
cd python && pytest tests/test_radio_debug_slave.py -v 2>&1 | tail -40
```

Expected: ALL tests in that file PASS, including the 2 new ones.

- [ ] **Step 8: Run full test suite to catch any other breakage**

```bash
cd python && pytest 2>&1 | tail -20
```

Expected: 591+2 = 593 passed, 1 skipped, 0 failed. (If anything else fails, fix before committing.)

- [ ] **Step 9: Commit**

```bash
git add python/feralrf/radio.py python/tests/test_radio_debug_slave.py
git commit -m "feat(f20.a.1.d): SlaveDbgResult parser rename + f21_first_nonzero + advA"
```

---

## Task 5: Run pre-commit and fix any lint issues

- [ ] **Step 1: Run pre-commit on staged + recent files**

```bash
pre-commit run --files \
  firmware/cc1352/src/radio_if.c \
  firmware/cc1352/include/radio_if.h \
  firmware/cc1352/src/command_processor.c \
  python/feralrf/radio.py \
  python/tests/test_radio_debug_slave.py
```

Expected: all checks pass (or auto-fix). If anything fails, fix manually and re-run before proceeding.

- [ ] **Step 2: If pre-commit modified files, amend/commit fixups**

```bash
git status
git add -u
git commit -m "chore(f20.a.1.d): pre-commit fixups" --allow-empty
```

(Use a new commit, not amend — feedback rule.)

---

## Task 6: Update smoke V2 to print new fields

**Files:**
- Modify: `python/examples/smoke_f20a1_b_diag.py` (`trace_table` helper)

- [ ] **Step 1: Locate the trace_table helper**

```bash
grep -n "trace_table\|f21_last_status\|last_tx_status\|advertise_iterations" python/examples/smoke_f20a1_b_diag.py
```

- [ ] **Step 2: Update field names and add 2 new rows**

In `trace_table` (or wherever the F20.a.1.c output rows are built), rename `last_tx_status` to `f21_last_status` and add:

```python
    rows.append((
        "f21_first_nonzero_status",
        f"0x{result.f21_first_nonzero_status:04X}",
        _interpret_ble_status(result.f21_first_nonzero_status)
        if result.f21_first_nonzero_status
        else "no non-OK iter — all 5000 returned BLE_DONE_OK",
    ))
    rows.append((
        "f21_adv_a",
        ":".join(f"{b:02X}" for b in reversed(result.f21_adv_a)),
        "AdvA bytes actually used by CMD_BLE_ADV — compare against Sniffle wire capture",
    ))
```

Helper (add near other `_interpret_*` helpers if absent):

```python
def _interpret_ble_status(status: int) -> str:
    return {
        0x1400: "BLE_DONE_OK",
        0x1402: "BLE_DONE_NOSYNC",
        0x1403: "BLE_DONE_RXERR",
        0x1404: "BLE_DONE_CONNECT (CSA#2)",
        0x140A: "BLE_DONE_CONNECT_CHSEL0 (legacy CSA)",
        0x140B: "BLE_DONE_ENDED",
        0x1810: "BLE_ERROR_PAR",
        0x1811: "BLE_ERROR_RXBUF",
    }.get(status, f"unknown 0x{status:04X}")
```

- [ ] **Step 3: Quick syntax check**

```bash
python -c "import ast; ast.parse(open('python/examples/smoke_f20a1_b_diag.py').read())"
```

Expected: no output (parse succeeded).

- [ ] **Step 4: Commit**

```bash
git add python/examples/smoke_f20a1_b_diag.py
git commit -m "test(f20.a.1.d): smoke V2 prints f21_first_nonzero_status + f21_adv_a"
```

---

## Task 7: Build firmware and run unit tests, sanity check

- [ ] **Step 1: Clean build**

```bash
cd firmware/cc1352 && rm -rf build && mkdir build && cd build
cmake .. && make -j$(nproc) 2>&1 | tail -10
```

Expected: `[100%] Built target feralrf_cc1352`. `feralrf_cc1352.hex` present.

- [ ] **Step 2: Verify CRC of new hex**

```bash
sha256sum firmware/cc1352/build/feralrf_cc1352.hex
```

Note the hash for traceability in the next task.

- [ ] **Step 3: Full Python test suite**

```bash
cd python && source .venv/bin/activate && pytest 2>&1 | tail -10
```

Expected: 593 passed, 1 skipped, 0 failed.

---

## Task 8: Flash both boards and validate clean evidence pipeline

**Pre-requisite:** Both CatSniffer CC1352P7 boards present. Identify ports:

```bash
python -m catnip devices
```

Note which device is #1 (master / initiator) vs #2 (slave / peripheral). Memory says ACM ports rotate per session — re-detect each time.

- [ ] **Step 1: Flash master (board #1)**

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

If first attempt fails, retry once before asking the user (per `feedback_flash_retry`). Expected: CRC verify OK + reset.

- [ ] **Step 2: Flash slave (board #2)**

```bash
python -m catnip flash -d 2 /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

Same retry rule.

- [ ] **Step 3: Run smoke V2 with low count to check parser end-to-end**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
python examples/smoke_f20a1_b_diag.py --count 200
```

Expected output should now include lines like:

```
f21_last_status              0x1400        BLE_DONE_OK
f21_first_nonzero_status     0x0000        no non-OK iter — all 200 returned BLE_DONE_OK
f21_adv_a                    XX:XX:XX:XX:XX:XX        AdvA bytes actually used by CMD_BLE_ADV — ...
advertise_iterations         200
```

**Validation — Phase 0 success criterion:** `f21_last_status` MUST now be a sane BLE status code (0x1400 / 0x1402 / 0x1404 / 0x140A) and consistent across runs (no longer leaking 433 PROP statuses or RANDOM other values from concurrent TX paths).

If the value is still random/leaking → Task 1 wiring is wrong; debug before proceeding.

- [ ] **Step 4: Commit smoke run logs as evidence**

Save the smoke output to `docs/superpowers/evidence/2026-05-08-f20a1d-phase0-smoke.txt` and commit:

```bash
mkdir -p docs/superpowers/evidence
python examples/smoke_f20a1_b_diag.py --count 200 > docs/superpowers/evidence/2026-05-08-f20a1d-phase0-smoke.txt 2>&1
git add docs/superpowers/evidence/2026-05-08-f20a1d-phase0-smoke.txt
git commit -m "test(f20.a.1.d): Phase 0 smoke — clean evidence pipeline validated"
```

---

## Task 9: Sniffle dual-capture procedure (Phase 1 — manual hardware step)

**Goal:** Capture wire-level both directions to compare AdvA fields against firmware report.

**Pre-requisite:** A 3rd CC1352 board flashed with Sniffle firmware, OR use one of the existing CatSniffers temporarily. If only 2 boards available, defer this task and use the master's own RX trace if Sniffle is non-trivial — but the master already telemeters CONNECT_IND fields which means it RX'd back its own TX or the radio reported TX completion, which is NOT a wire capture.

- [ ] **Step 1: Set up Sniffle on a 3rd board**

If you have a 3rd board: flash Sniffle (https://github.com/nccgroup/Sniffle), open `python/sniff_receiver.py -c 37 -e -o capture.pcap`. Confirm advertising packets stream in real time.

If you don't have a 3rd board, **STOP** and ask the user how to proceed — the alternative is a logic analyzer on the SMA, which is out of scope for this plan.

- [ ] **Step 2: Run smoke + Sniffle in parallel**

Open two terminals:

Terminal A (Sniffle, channel 37):
```bash
python sniff_receiver.py -c 37 -e -o /tmp/f20a1d-capture.pcap
```

Terminal B (FeralRF smoke):
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
python examples/smoke_f20a1_b_diag.py --count 5000 > /tmp/f20a1d-smoke.txt
```

Wait for smoke to finish (~50 s). Stop Sniffle (`Ctrl-C`).

- [ ] **Step 3: Inspect capture in Wireshark**

```bash
wireshark /tmp/f20a1d-capture.pcap &
```

Filter: `btle.advertising_header.pdu_type == 0` (ADV_IND).

Pick a recent ADV_IND. Note its **AdvA** field (6 bytes, little-endian on wire).

Filter: `btle.advertising_header.pdu_type == 5` (CONNECT_IND).

Find the CONNECT_IND that immediately follows an ADV_IND from the same AdvA. Note:
- **InitA** (initiator address)
- **AdvA** (target — should match slave's AdvA)
- **AA** (assigned access address — already known: 0x85D73635 from F20.a.1.c)
- **Header byte 0**, specifically bit 5 (ChSel#2 indicator)

- [ ] **Step 4: Compare against firmware report**

In `/tmp/f20a1d-smoke.txt`, find the `f21_adv_a` line.

**Decision matrix:**

| Wire AdvA (CONNECT_IND target) | Firmware f21_adv_a | Conclusion |
|--------------------------------|--------------------|----|
| Match | Match | AdvA is NOT the issue → proceed to Task 10 ChSel branch |
| Different | (firmware byte order) | AdvA mismatch found → Task 10 fix branch |
| Wire AdvA absent in CONNECT_IND | (any) | Master is targeting a different slave → check master's `peer_addr` config |

Document the finding in `docs/superpowers/evidence/2026-05-08-f20a1d-phase1-advA-comparison.md` with screenshots from Wireshark and the smoke output.

- [ ] **Step 5: Commit Phase 1 evidence**

```bash
git add docs/superpowers/evidence/2026-05-08-f20a1d-phase1-advA-comparison.md /tmp/f20a1d-capture.pcap  # if pcap small enough
git commit -m "test(f20.a.1.d): Phase 1 Sniffle dual-capture evidence"
```

(If pcap is too large for git, store path in the markdown but don't commit the binary.)

---

## Task 10: Branch on Phase 1 result

This task has two mutually exclusive paths. Choose based on Task 9 Step 4 conclusion.

### Path A: AdvA mismatch found

- [ ] **Step 1A: Identify the mismatch source**

Inspect:
- Python `radio.set_ble_addr()` call in smoke V2 (what bytes are passed?)
- `RadioIF_setBleAdvAddress` in radio_if.c:2205 (does it propagate to F21 path?)
- `RadioIF_transmitBleAdvLegacy` `addr` parameter chain — does the master and slave actually agree on the slave's address?

```bash
grep -n "set_ble_addr\|setBleAdvAddress\|RadioIF_transmitBleAdvLegacy" \
    python/feralrf/radio.py firmware/cc1352/src/radio_if.c
```

- [ ] **Step 2A: Fix the mismatch**

Most likely fix locations:
- Python smoke V2 sets the wrong address.
- Master's `peer_addr` in `Radio.ble_connect()` doesn't match slave's actual `addr`.
- Endianness mismatch (Python sends MSB-first, firmware expects LSB-first).

Whatever the fix, write a test in `python/tests/test_radio_debug_slave.py` or `python/tests/test_smoke_f20a1.py` that pins down the expected address bytes.

- [ ] **Step 3A: Re-run smoke V2 with count=200**

Expect: `f21_last_status = 0x1404 (BLE_DONE_CONNECT)` or `0x140A`. `advertise_iterations < 200` (broke out early on connect). Slave should now be in conn state.

- [ ] **Step 4A: Re-run with count=5000 for endurance**

Expect: connection sustains ≥10 events (matching F8a sustain criterion). If yes, F20.a.1 is fixed.

- [ ] **Step 5A: Tag and update memory**

```bash
git tag v2.0-f20.a.1
git push origin feature/f20a1-peripheral-read --tags
```

Update `~/.claude/projects/.../memory/MEMORY.md` and write `project_f20a1_done.md`.

### Path B: AdvA matches → write F20.a.1.e plan stub

- [ ] **Step 1B: Document Phase 1 conclusion**

Append to `docs/superpowers/evidence/2026-05-08-f20a1d-phase1-advA-comparison.md`:

> AdvA wire-side and firmware report match. AdvA mismatch is RULED OUT as root cause.
> Next hypothesis to test: ChSel#2 mismatch (master is BLE5, slave uses BLE 4.x CMD_BLE_ADV).
> Evidence to collect in F20.a.1.e: ChSel bit (header byte 0, bit 5) of CONNECT_IND from Sniffle pcap.

If the Sniffle inspection in Task 9 already showed ChSel=1 in CONNECT_IND, note that as the smoking gun.

- [ ] **Step 2B: Write F20.a.1.e plan stub**

Create `docs/superpowers/plans/2026-05-08-f20a1e-bleN-cmd-or-timing.md` with:
- Hypothesis 2 detail: switch F21 to `CMD_BLE5_ADV_LEGACY` if it exists in TI SDK 8.30, OR force master to clear ChSel bit before sending CONNECT_IND.
- Hypothesis 3 detail: replace Task_sleep with chained CMD_BLE_ADV using endTrigger/endTime.
- Note the `radio_if.c:577` warning: `multi_protocol does NOT support CMD_BLE5_ADV_AUX (hangs)` — investigate whether `CMD_BLE5_ADV_LEGACY` (different opcode) shares the same patch issue.

- [ ] **Step 3B: Update F20.a.1.c memory**

Edit `~/.claude/projects/.../memory/project_f20a1c_partial.md` with a closing note: "F20.a.1.d superseded F20.a.1.c by adding clean evidence; AdvA ruled out, next vuelta is F20.a.1.e (ChSel#2 / timing)."

- [ ] **Step 4B: Tag F20.a.1.d as a partial checkpoint**

```bash
git tag v2.0-f20.a.1.d-partial
git push origin feature/f20a1-peripheral-read --tags
```

Branch held — no FF to main.

---

## Task 11: Final validation and PR-style review (Path A only)

Skip this task if executing Path B above.

- [ ] **Step 1: Run full Python test suite**

```bash
cd python && pytest 2>&1 | tail -10
```

Expected: 593+ passed, 0 failed.

- [ ] **Step 2: 3 endurance smoke runs**

```bash
for i in 1 2 3; do
    echo "=== Run $i ==="
    python examples/smoke_f20a1_b_diag.py --count 5000
done > /tmp/f20a1d-endurance.txt 2>&1
```

Expected: all 3 runs successful; sustain ≥ 10 events each.

- [ ] **Step 3: Update memory + finalize**

```bash
# Write project_f20a1_done.md and update MEMORY.md index
# (handled by the agent as part of normal workflow)
```

- [ ] **Step 4: FF to feature/ti-rtos-migration if applicable**

Coordinate with user — F20.a.1 closure may need to FF or PR to the integration branch per project_f8f_done workflow.

---

## Self-Review Checklist (run after writing this plan)

- [x] **Spec coverage:** Every section of the F20.a.1.c findings (confounder fix, AdvA capture, Sniffle verification, conditional fix paths) has a concrete task.
- [x] **Placeholder scan:** No "TBD" / "implement later". One conditional ("Path A or Path B") with explicit branch instructions in Tasks 10–11.
- [x] **Type consistency:** `f21_last_status` (Python) ↔ `f21LastStatus` (C) consistent across Tasks 1–6. Wire offsets match comment block in Task 3 ↔ parser in Task 4.
- [x] **Ambiguity check:** Wire layout offsets are explicit. The only judgment call (Path A vs B in Task 10) has explicit decision criteria in Task 9 Step 4.

## Out of Scope

- Switching `CMD_BLE_ADV` (BLE 4.x) → `CMD_BLE5_ADV_LEGACY` (BLE 5). Deferred to F20.a.1.e.
- Eliminating Task_sleep blind window. Deferred to F20.a.1.e.
- Address randomization (RPA) handling. Deferred to F20.b.
- SMP / pairing. Deferred to a separate phase outside F20.

## Done Criteria

**Phase 0 done when:**
- Firmware builds clean (Task 7).
- Python tests 593+ passing (Task 4 Step 8).
- Smoke V2 reports a sane BLE status code in `f21_last_status` that does not change between runs unrelated to F21 ADV (Task 8 Step 3).

**Phase 1 done when:**
- Sniffle pcap captured with both ADV_IND and CONNECT_IND (Task 9 Step 2).
- AdvA comparison documented as Match or Mismatch in `docs/superpowers/evidence/2026-05-08-f20a1d-phase1-advA-comparison.md` (Task 9 Step 4).

**F20.a.1 closed (Path A) when:** smoke V2 with count=5000 yields `f21_last_status = 0x1404/0x140A` and connection sustains ≥10 events (Task 10A Step 4). Tag `v2.0-f20.a.1` pushed.

**F20.a.1.d closed as PARTIAL (Path B) when:** Phase 0 + Phase 1 evidence committed, F20.a.1.e plan stub written, tag `v2.0-f20.a.1.d-partial` pushed.
