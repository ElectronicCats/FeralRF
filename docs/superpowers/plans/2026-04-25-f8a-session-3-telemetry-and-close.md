# F8A Session 3 — Telemetry-driven NOSYNC fix + F8A closeout

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land per-event RAT-tick telemetry, correlate it with a Sniffle pcap to compute the actual master-TX-vs-peer-RX offset, apply the resulting timing fix, and close F8A entirely (GATT discover+read end-to-end, ICall cleanup, regression 8/8 PHYs, tag `v2.0-f8a`).

**Architecture:** Firmware grows one new request/response pair (`CMD_DEBUG_TIMING` / `RSP_DEBUG_TIMING`) backed by a ring buffer in `ble_conn_mgr.c` that captures `(eventIdx, startRAT, endRAT, status, numSent)` per master event. Python `Radio.debug_timing()` parses the buffer into a list of dataclasses. A one-shot offline script joins one full connection's timing buffer with the matching Sniffle pcap to compute the wall-clock delta between our master TX and the peer's listening window, which becomes the input to a single deterministic fix in `BleConnMgr_start`. After the fix sustains a connection, GATT discover+read close F8 inline, ICall residue is removed, and the branch is tagged.

**Tech Stack:** TI SimpleLink CC13xx/CC26xx SDK 8.30.01.01 (TI-RTOS 7), CC1352P7, GCC arm-none-eabi, Python 3.10+, pyserial, COBS+CRC16 protocol, Sniffle CC1352P7 1M firmware as on-air oracle, catnip flasher, pytest.

**Branch / starting HEAD:** `feature/f8a-ble-central-sniffle` @ `fcb016f`

**Hardware setup (do NOT change unless task says so):**
- Board #1 — CatSniffer 504B32 (IEEE `00:12:4B:00:2A:79:BF:F1`, CC1352P7) on `/dev/ttyACM0`, FeralRF under test.
- Board #2 — CatSniffer 565932 (IEEE `00:12:4B:00:2A:79:C1:82`, broken antenna RX-only at short range) on `/dev/ttyACM1`, Sniffle CC1352P7 1M (catnip alias `sniffle`).
- Target peer — CH573 BLE 4.2 at `DC:32:62:8D:E1:09` (public, conn interval 30 ms). MUST be powered before any Tx tasks.

**Reference paths (used throughout the plan):**
- Catnip CLI: `/home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py`
- Sniffle host CLI: `/home/sabas/Documents/electroniccats/Sniffle/python_cli/sniff_receiver.py`
- Plan dir: `docs/superpowers/plans/`
- Investigation dir for Session 3: `docs/investigations/2026-04-24-f8a-session-1/` (continue using same dir; new artifacts get session-3 prefix)

**Constraints carried from previous sessions (DO NOT touch unless telemetry tells us to):**
- Keep `bDynamicWinOffset = 1` in `BleConn_initiate` (`firmware/cc1352/src/ble_conn.c:154`).
- Keep the Sniffle-style anchor formula `nextHopTime = connTime - AO_TARG + hopIntervalTicks` (`firmware/cc1352/src/ble_conn_mgr.c:184`) until §T3 produces evidence.
- Do NOT re-run H1 (skip `RF_cancelCmd+RF_flushCmd`), H2 (WinSize 3→10), or H3 (alternate formula + `bDynamicWinOffset=0`); all three were falsified with the Sniffle oracle in Session 2.
- Do NOT attempt a manual `CMD_BLE5_GENERIC_TX` rewrite — the opcode does not exist in SDK 8.30 and Sniffle does not use it either.
- Pre-commit MUST run on every commit (no `--no-verify`).
- Flash with `.hex` only, never `.bin`.
- Retry `catnip flash` 2× before asking the user to power-cycle a board.

---

## File map (created or modified by this plan)

| File | Action | Responsibility |
|------|--------|----------------|
| `firmware/cc1352/src/ble_conn_mgr.c` | Modify | Add `s_dbg_timing[]` ring buffer + `BleConnMgr_getDebugTiming()`. Apply timing fix in T4. |
| `firmware/cc1352/include/ble_conn_mgr.h` | Modify | Export `BleConnMgr_getDebugTiming` + struct + `BLE_CONN_MGR_DBG_TIMING_DEPTH`. |
| `firmware/cc1352/src/command_processor.c` | Modify | Add `CMD_DEBUG_TIMING` (0x47) + `RSP_DEBUG_TIMING` (0xA8) handler. |
| `python/feralrf/enums.py` | Modify | Add `Command.DEBUG_TIMING = 0x47`, `Response.DEBUG_TIMING = 0xA8`. |
| `python/feralrf/commands.py` | Modify | Add `CommandBuilder.debug_timing()`. |
| `python/feralrf/_responses.py` | Modify | Add `DebugTimingEntry` + `DebugTimingResponse` dataclasses. |
| `python/feralrf/radio.py` | Modify | Add `Radio.debug_timing()` returning `DebugTimingResponse`. |
| `python/tests/test_debug_timing.py` | Create | Contract tests for command building + response parsing. |
| `python/examples/lab/f8a_session3_capture.py` | Create | End-to-end run script: fire a connect, dump conn_status + debug_timing, save JSON. |
| `python/examples/lab/f8a_session3_offset_analysis.py` | Create | Offline correlator: read Sniffle pcap + JSON dump → compute master-TX-vs-peer-RX offset in RAT ticks. |
| `docs/investigations/2026-04-24-f8a-session-1/session-3-capture-fixed.json` | Create (T2) | Raw timing buffer of one captured connection. |
| `docs/investigations/2026-04-24-f8a-session-1/session-3-sniffle.pcap` | Create (T2) | Companion Sniffle pcap of the same connection. |
| `docs/investigations/2026-04-24-f8a-session-1/session-3-closeout.md` | Create (T6) | Closeout report (parallels session-1 / session-2). |
| `firmware/cc1352/startup/osal_icall_ble.c` | Delete (T6) | ICall residue from BLE5-Stack experiment. |
| `firmware/cc1352/syscfg/ti_ble_config.c` | Delete (T6) | ICall residue. |
| `firmware/cc1352/syscfg/ti_ble_config.h` | Delete (T6) | ICall residue. |
| `firmware/cc1352/src/main_rtos.c` | Modify (T6) | Drop `ICall_init` / `ICall_createRemoteTasks` / `appServiceInfo->timerMaxMillisecond` block (lines 216-221). |
| `firmware/cc1352/CMakeLists.txt` | Modify (T6) | Drop ICall sources from build target list. |
| `firmware/cc1352/src/rtos_stubs.c` | Modify (T6) | Drop `ICall_getMaxMSecs` stub if no longer referenced. |

---

## Task 1 — Telemetry plumbing (`CMD_DEBUG_TIMING` / `RSP_DEBUG_TIMING`)

Goal: After a connect attempt finishes (success or NOSYNC), the host can pull the last 16 master events with their RAT timestamps and outcome.

**Files:**
- Modify: `firmware/cc1352/include/ble_conn_mgr.h`
- Modify: `firmware/cc1352/src/ble_conn_mgr.c`
- Modify: `firmware/cc1352/src/command_processor.c`
- Modify: `python/feralrf/enums.py`
- Modify: `python/feralrf/commands.py`
- Modify: `python/feralrf/_responses.py`
- Modify: `python/feralrf/radio.py`
- Create: `python/tests/test_debug_timing.py`

### T1.1 — Write the failing Python contract test

- [ ] **Step 1: Create the test file**

```python
# python/tests/test_debug_timing.py
"""Contract tests for CMD_DEBUG_TIMING / RSP_DEBUG_TIMING."""

import struct

from feralrf.commands import CommandBuilder
from feralrf.enums import Command, Response
from feralrf._responses import DebugTimingEntry, DebugTimingResponse


def test_command_id_and_response_id_are_in_enum():
    assert Command.DEBUG_TIMING == 0x47
    assert Response.DEBUG_TIMING == 0xA8


def test_command_builder_has_no_payload():
    assert CommandBuilder.debug_timing() == b""


def test_response_parses_zero_entries():
    payload = bytes([0])  # count=0, no entries
    parsed = DebugTimingResponse.parse(payload)
    assert parsed.count == 0
    assert parsed.entries == []


def test_response_parses_two_entries():
    e1 = struct.pack("<HIIHB", 0, 0x10000000, 0x10100000, 0x1402, 0)
    e2 = struct.pack("<HIIHB", 1, 0x10100000, 0x10200000, 0x1400, 1)
    payload = bytes([2]) + e1 + e2
    parsed = DebugTimingResponse.parse(payload)
    assert parsed.count == 2
    assert parsed.entries == [
        DebugTimingEntry(event_idx=0, start_rat=0x10000000, end_rat=0x10100000,
                         status=0x1402, num_sent=0),
        DebugTimingEntry(event_idx=1, start_rat=0x10100000, end_rat=0x10200000,
                         status=0x1400, num_sent=1),
    ]


def test_response_rejects_truncated_entry():
    payload = bytes([1]) + b"\x00\x00"  # claims 1 entry, truncated
    try:
        DebugTimingResponse.parse(payload)
    except ValueError:
        return
    raise AssertionError("expected ValueError on truncated payload")
```

- [ ] **Step 2: Run the test, expect failure**

Run: `cd python && source .venv/bin/activate && pytest tests/test_debug_timing.py -v`
Expected: 4 failures — `Command.DEBUG_TIMING`, `Response.DEBUG_TIMING`, `CommandBuilder.debug_timing`, `DebugTimingEntry`/`DebugTimingResponse` not yet defined.

### T1.2 — Add Python enums

- [ ] **Step 1: Edit `python/feralrf/enums.py`**

Inside `class Command(IntEnum)`, after `GATT_WRITE = 0x46`, before the closing of the class, add:

```python
    # Diagnostics
    DEBUG_TIMING = 0x47
```

Inside `class Response(IntEnum)`, after `GATT_DONE = 0xA5`, add:

```python
    # Diagnostics
    DEBUG_TIMING = 0xA8
```

- [ ] **Step 2: Re-run the test, partial pass expected**

Run: `pytest tests/test_debug_timing.py::test_command_id_and_response_id_are_in_enum -v`
Expected: PASS.

### T1.3 — Add `CommandBuilder.debug_timing`

- [ ] **Step 1: Edit `python/feralrf/commands.py`**

Inside `CommandBuilder`, after `gatt_write`, add:

```python
    @staticmethod
    def debug_timing() -> bytes:
        """No payload for CMD_DEBUG_TIMING."""
        return b""
```

- [ ] **Step 2: Re-run the test**

Run: `pytest tests/test_debug_timing.py::test_command_builder_has_no_payload -v`
Expected: PASS.

### T1.4 — Add `DebugTimingEntry` + `DebugTimingResponse`

- [ ] **Step 1: Edit `python/feralrf/_responses.py`**

At the bottom of the file, append:

```python
@dataclass
class DebugTimingEntry:
    """One captured master-event timing record (matches firmware ring entry)."""

    event_idx: int       # u16 — BleConnMgr s_event_counter at capture time
    start_rat: int       # u32 — curHopTime fed to RadioIF_bleCentral
    end_rat: int         # u32 — s_next_hop_time fed to RadioIF_bleCentral
    status: int          # u16 — RF status code (BLE_DONE_NOSYNC=0x1402, OK=0x1400, …)
    num_sent: int        # u8  — nTxEntryDone returned by the command


@dataclass
class DebugTimingResponse:
    """Parsed RSP_DEBUG_TIMING payload: 1-byte count + count×11-byte entries."""

    count: int
    entries: list

    _ENTRY_SIZE = 13  # u16 + u32 + u32 + u16 + u8

    @classmethod
    def parse(cls, payload: bytes) -> "DebugTimingResponse":
        if len(payload) < 1:
            raise ValueError("DEBUG_TIMING payload too short (no count byte)")
        count = payload[0]
        expected_len = 1 + count * cls._ENTRY_SIZE
        if len(payload) < expected_len:
            raise ValueError(
                f"DEBUG_TIMING payload truncated: got {len(payload)}, "
                f"need {expected_len} for count={count}"
            )
        entries = []
        for i in range(count):
            base = 1 + i * cls._ENTRY_SIZE
            event_idx, start_rat, end_rat, status, num_sent = struct.unpack(
                "<HIIHB", payload[base:base + cls._ENTRY_SIZE]
            )
            entries.append(DebugTimingEntry(
                event_idx=event_idx, start_rat=start_rat,
                end_rat=end_rat, status=status, num_sent=num_sent,
            ))
        return cls(count=count, entries=entries)
```

- [ ] **Step 2: Re-run the full test file**

Run: `pytest tests/test_debug_timing.py -v`
Expected: 4 PASS.

### T1.5 — Add `Radio.debug_timing`

- [ ] **Step 1: Edit `python/feralrf/radio.py`**

In the imports near the top (find the existing `_responses` import block), add `DebugTimingResponse` to the imported names.

- [ ] **Step 2: Add the method**

After `conn_status` (around line 620), insert:

```python
    def debug_timing(self, timeout: float = 2.0) -> "DebugTimingResponse":
        """Issue CMD_DEBUG_TIMING; firmware returns the last N master-event timing records."""
        from feralrf._responses import DebugTimingResponse
        self._send_command(Command.DEBUG_TIMING, CommandBuilder.debug_timing())
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.DEBUG_TIMING, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("DEBUG_TIMING failed", payload[0] if payload else 0)
        if cmd_id != Response.DEBUG_TIMING:
            raise ProtocolError(f"Unexpected response to DEBUG_TIMING: 0x{cmd_id:02X}")
        return DebugTimingResponse.parse(payload)
```

- [ ] **Step 3: Smoke-import**

Run: `cd python && python -c "from feralrf import Radio; from feralrf._responses import DebugTimingResponse; print('ok')"`
Expected: `ok`.

### T1.6 — Add firmware ring buffer

- [ ] **Step 1: Edit `firmware/cc1352/include/ble_conn_mgr.h`**

After the existing function declarations, add:

```c
#define BLE_CONN_MGR_DBG_TIMING_DEPTH 16u

/* One captured master-event timing record. Layout is wire-stable: see
 * RSP_DEBUG_TIMING in command_processor.c and python/feralrf/_responses.py. */
typedef struct {
    uint16_t eventIdx;   /* s_event_counter at capture time */
    uint32_t startRAT;   /* curHopTime fed to RadioIF_bleCentral */
    uint32_t endRAT;     /* s_next_hop_time fed to RadioIF_bleCentral */
    uint16_t status;     /* RF status code returned by the command */
    uint8_t  numSent;    /* nTxEntryDone returned by the command */
} BleConnMgr_DbgTimingEntry;

/* Returns up to maxEntries snapshots of the most recent master events,
 * oldest first. The returned count equals min(active entries, maxEntries). */
uint8_t BleConnMgr_getDebugTiming(BleConnMgr_DbgTimingEntry *out, uint8_t maxEntries);
```

- [ ] **Step 2: Edit `firmware/cc1352/src/ble_conn_mgr.c` — module statics**

Find the static-state block (around lines 42-52). At the END of that block (right after `static uint32_t s_dbg_total_tx_done;`), add:

```c
/* Debug timing ring buffer — populated each call to BleConnMgr_poll().
 * Cleared on BleConnMgr_start so each connect attempt sees a fresh log. */
static BleConnMgr_DbgTimingEntry s_dbg_timing[BLE_CONN_MGR_DBG_TIMING_DEPTH];
static uint8_t  s_dbg_timing_head;   /* next write slot 0..DEPTH-1 */
static uint8_t  s_dbg_timing_count;  /* number of valid entries (saturates at DEPTH) */
```

- [ ] **Step 3: Clear the ring on `BleConnMgr_start`**

Find the body of `BleConnMgr_start` (around lines 165-201). Right after `s_dbg_total_tx_done = 0;` (around line 175), add:

```c
    s_dbg_timing_head = 0;
    s_dbg_timing_count = 0;
```

- [ ] **Step 4: Record an entry inside `BleConnMgr_poll`**

Find the block in `BleConnMgr_poll` that already computes `startTime`, `endTime`, runs `RadioIF_bleCentral`, and stores the result (around lines 249-256). RIGHT AFTER `s_dbg_total_tx_done += numSent;` and BEFORE `TXQueue_flush(numSent);`, add:

```c
    /* Snapshot timing for host-side correlation (Session 3 telemetry). */
    {
        BleConnMgr_DbgTimingEntry *e = &s_dbg_timing[s_dbg_timing_head];
        e->eventIdx = s_event_counter;
        e->startRAT = startTime;
        e->endRAT = endTime;
        e->status = (uint16_t)status;
        e->numSent = (uint8_t)numSent;
        s_dbg_timing_head =
            (uint8_t)((s_dbg_timing_head + 1u) % BLE_CONN_MGR_DBG_TIMING_DEPTH);
        if (s_dbg_timing_count < BLE_CONN_MGR_DBG_TIMING_DEPTH) {
            s_dbg_timing_count++;
        }
    }
```

- [ ] **Step 5: Add `BleConnMgr_getDebugTiming` to the public API**

At the bottom of `firmware/cc1352/src/ble_conn_mgr.c` (after `BleConnMgr_getLastStatus`), append:

```c
uint8_t BleConnMgr_getDebugTiming(BleConnMgr_DbgTimingEntry *out, uint8_t maxEntries) {
    if (out == NULL || maxEntries == 0u) {
        return 0u;
    }
    uint8_t n = (s_dbg_timing_count < maxEntries) ? s_dbg_timing_count : maxEntries;

    /* Walk oldest-to-newest. With a saturating ring of DEPTH entries,
     * the oldest slot is (head - count) mod DEPTH. */
    uint8_t start = (uint8_t)((BLE_CONN_MGR_DBG_TIMING_DEPTH + s_dbg_timing_head
                               - s_dbg_timing_count) % BLE_CONN_MGR_DBG_TIMING_DEPTH);
    for (uint8_t i = 0; i < n; i++) {
        out[i] = s_dbg_timing[(start + i) % BLE_CONN_MGR_DBG_TIMING_DEPTH];
    }
    return n;
}
```

### T1.7 — Wire `CMD_DEBUG_TIMING` into the command processor

- [ ] **Step 1: Edit `firmware/cc1352/src/command_processor.c` — IDs**

After the existing GATT command defines (around line 48), add:

```c
/* Diagnostics */
#define CMD_DEBUG_TIMING 0x47u
```

After `RSP_GATT_DONE` (around line 64), add:

```c
/* Diagnostics */
#define RSP_DEBUG_TIMING 0xA8u
```

- [ ] **Step 2: Add the case to `handle_command`**

Find the `switch (cmd)` block. After the `CMD_GATT_WRITE` case (which ends around line 559), and BEFORE `default:`, add:

```c
    case CMD_DEBUG_TIMING: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        /* Wire layout: count(u8) + count × { eventIdx(u16) startRAT(u32)
         * endRAT(u32) status(u16) numSent(u8) }  →  1 + 16*13 = 209 bytes max. */
        uint8_t rsp[1u + BLE_CONN_MGR_DBG_TIMING_DEPTH * 13u];
        BleConnMgr_DbgTimingEntry entries[BLE_CONN_MGR_DBG_TIMING_DEPTH];
        uint8_t n = BleConnMgr_getDebugTiming(entries, BLE_CONN_MGR_DBG_TIMING_DEPTH);
        rsp[0] = n;
        for (uint8_t i = 0; i < n; i++) {
            uint8_t *p = &rsp[1u + (uint16_t)i * 13u];
            p[0] = (uint8_t)(entries[i].eventIdx & 0xFFu);
            p[1] = (uint8_t)(entries[i].eventIdx >> 8);
            p[2] = (uint8_t)(entries[i].startRAT & 0xFFu);
            p[3] = (uint8_t)((entries[i].startRAT >> 8) & 0xFFu);
            p[4] = (uint8_t)((entries[i].startRAT >> 16) & 0xFFu);
            p[5] = (uint8_t)((entries[i].startRAT >> 24) & 0xFFu);
            p[6] = (uint8_t)(entries[i].endRAT & 0xFFu);
            p[7] = (uint8_t)((entries[i].endRAT >> 8) & 0xFFu);
            p[8] = (uint8_t)((entries[i].endRAT >> 16) & 0xFFu);
            p[9] = (uint8_t)((entries[i].endRAT >> 24) & 0xFFu);
            p[10] = (uint8_t)(entries[i].status & 0xFFu);
            p[11] = (uint8_t)((entries[i].status >> 8) & 0xFFu);
            p[12] = entries[i].numSent;
        }
        send_response(RSP_DEBUG_TIMING, seq, rsp, (uint16_t)(1u + (uint16_t)n * 13u));
        return;
    }
```

### T1.8 — Build firmware

- [ ] **Step 1: Build**

Run: `cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -20`
Expected: `feralrf_cc1352.hex` regenerated, no warnings, no errors. If a header was missed, `make` will name the file.

- [ ] **Step 2: Confirm `.hex` exists**

Run: `ls -la /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex`
Expected: file exists, mtime equals just-now.

### T1.9 — Flash board #1 and smoke-test

- [ ] **Step 1: Flash**

Run: `python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py flash -d 1 /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex`
Expected: catnip prints success. Retry up to 2× if it fails before asking for a manual reset.

- [ ] **Step 2: Idle smoke test — debug_timing returns count=0**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && timeout 6 python3 -c "
from feralrf import Radio
r = Radio('/dev/ttyACM0'); r.connect(); r.init()
dt = r.debug_timing()
print('count=', dt.count, 'entries=', dt.entries)
r.close()
"
```
Expected: `count= 0 entries= []` (no connect attempt yet — buffer empty).

### T1.10 — Commit T1

- [ ] **Step 1: Run pre-commit**

Run: `cd /home/sabas/Documents/electroniccats/FeralRF && pre-commit run --files firmware/cc1352/src/ble_conn_mgr.c firmware/cc1352/include/ble_conn_mgr.h firmware/cc1352/src/command_processor.c python/feralrf/enums.py python/feralrf/commands.py python/feralrf/_responses.py python/feralrf/radio.py python/tests/test_debug_timing.py`
Expected: PASS (or auto-fix that we re-stage).

- [ ] **Step 2: Stage + commit**

```bash
git add firmware/cc1352/include/ble_conn_mgr.h \
        firmware/cc1352/src/ble_conn_mgr.c \
        firmware/cc1352/src/command_processor.c \
        python/feralrf/enums.py \
        python/feralrf/commands.py \
        python/feralrf/_responses.py \
        python/feralrf/radio.py \
        python/tests/test_debug_timing.py
git commit -m "feat(f8a): add CMD_DEBUG_TIMING / RSP_DEBUG_TIMING (0x47/0xA8)

Per-master-event RAT-tick ring buffer (depth 16) populated by
BleConnMgr_poll. Wire layout: count(u8) + count × (eventIdx u16,
startRAT u32, endRAT u32, status u16, numSent u8). Python exposes it
via Radio.debug_timing(). Used by Session 3 to correlate our master TX
timing with a Sniffle pcap of the same connection."
```

---

## Task 2 — Capture one full connection (FeralRF telemetry + Sniffle pcap)

Goal: Produce a pair of artifacts (`session-3-capture-fixed.json`, `session-3-sniffle.pcap`) that record the same connection attempt from both sides.

### T2.1 — Pre-flight hardware check

- [ ] **Step 1: Verify both boards present**

Run: `python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py devices`
Expected: 2 boards listed at `/dev/ttyACM0` (504B32, FeralRF under test) and `/dev/ttyACM1` (565932).

- [ ] **Step 2: Confirm board #2 still has Sniffle**

If catnip output does NOT report `/dev/ttyACM1` as Sniffle, run:
`python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py flash -d 2 sniffle`
Expected: success. Retry up to 2×.

- [ ] **Step 3: Confirm CH573 is advertising**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && timeout 6 python3 -c "
from feralrf import Radio; from feralrf.enums import PHY
import time
r = Radio('/dev/ttyACM0'); r.connect(); r.init()
r.set_phy(PHY.BLE_1M, 37, 2402_000_000); r.start_rx()
time.sleep(3); pkts = list(r.read_packets()); r.stop_rx()
target = bytes.fromhex('09E18D6232DC')
print(f'CH573 ADV_IND: {sum(1 for p in pkts if target in p.data)}/{len(pkts)}')
r.close()
"
```
Expected: `CH573 ADV_IND: <hits>/<total>` with hits > 0. If 0, the operator powers CH573 and re-runs the step.

### T2.2 — Capture script

- [ ] **Step 1: Create `python/examples/lab/f8a_session3_capture.py`**

```python
"""F8A Session 3 — capture one connection attempt.

Runs CMD_CONNECT against CH573, lets the firmware burn through master
events until supervisionTimeout drops the link, then dumps:

    {
      "conn_result": int,
      "conn_status": <ConnectionStatus.__dict__ snapshot>,
      "debug_timing": [<DebugTimingEntry.__dict__>...],
      "wallclock_capture_start_unix_ns": int,
      "wallclock_capture_end_unix_ns":   int,
    }

The wallclock fields anchor the capture to the same UNIX time base as
the Sniffle pcap (which records pcap-standard ns timestamps).
"""

import argparse
import dataclasses
import json
import time
from pathlib import Path

from feralrf import Radio


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--target", default="DC:32:62:8D:E1:09")
    parser.add_argument("--addr-type", type=int, default=0)
    parser.add_argument("--out", required=True)
    parser.add_argument("--linger", type=float, default=2.5,
                        help="Seconds to wait after CONN_RESULT before dumping telemetry.")
    args = parser.parse_args()

    r = Radio(args.port)
    r.connect()
    r.init()

    t0 = time.time_ns()
    res = r.ble_connect(args.target, addr_type=args.addr_type)
    time.sleep(args.linger)
    status = r.conn_status()
    timing = r.debug_timing()
    t1 = time.time_ns()

    try:
        r.ble_disconnect()
    except Exception:
        pass
    r.close()

    out = {
        "conn_result": int(res.result),
        "conn_status": dataclasses.asdict(status),
        "debug_timing": [dataclasses.asdict(e) for e in timing.entries],
        "wallclock_capture_start_unix_ns": t0,
        "wallclock_capture_end_unix_ns": t1,
    }
    Path(args.out).write_text(json.dumps(out, indent=2))
    print(f"wrote {args.out}: result={res.result} events={status.events} "
          f"timing_count={timing.count}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Smoke-run with no Sniffle (just verify the script works)**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && python3 examples/lab/f8a_session3_capture.py \
  --out /tmp/feralrf-session3-smoke.json
cat /tmp/feralrf-session3-smoke.json | python3 -m json.tool | head -40
```
Expected: JSON dump with `events ≥ 1`, `debug_timing` array of ≥1 entry with `status: 5122` (`0x1402` decimal = `BLE_DONE_NOSYNC`).

### T2.3 — Run paired capture

- [ ] **Step 1: Start Sniffle in the background**

Run (in a separate terminal or with `&`):
```bash
python3 /home/sabas/Documents/electroniccats/Sniffle/python_cli/sniff_receiver.py \
  -s /dev/ttyACM1 -c 37 -m DC:32:62:8D:E1:09 \
  -o /home/sabas/Documents/electroniccats/FeralRF/docs/investigations/2026-04-24-f8a-session-1/session-3-sniffle.pcap &
echo $! > /tmp/sniffle.pid
sleep 2
```
Expected: Sniffle prints "Listening". `/tmp/sniffle.pid` holds its PID.

- [ ] **Step 2: Run the capture script**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && python3 examples/lab/f8a_session3_capture.py \
  --out /home/sabas/Documents/electroniccats/FeralRF/docs/investigations/2026-04-24-f8a-session-1/session-3-capture-fixed.json
```
Expected: `result=0 events=<n> timing_count=<m>` with both `n` and `m` > 0.

- [ ] **Step 3: Stop Sniffle**

Run: `kill -INT $(cat /tmp/sniffle.pid); sleep 1; rm /tmp/sniffle.pid`
Expected: Sniffle exits cleanly, pcap file present and non-empty.

- [ ] **Step 4: Verify both artifacts**

Run:
```bash
ls -la /home/sabas/Documents/electroniccats/FeralRF/docs/investigations/2026-04-24-f8a-session-1/session-3-*
```
Expected: both `session-3-capture-fixed.json` (≥ 500 B) and `session-3-sniffle.pcap` (≥ 5 KB) present.

---

## Task 3 — Offset analysis + fix decision

Goal: Compute, in RAT ticks, the actual offset between our first master-TX (`startRAT` of event 0) and the peer's first listening window (Sniffle-observed CONNECT_IND end + transmitWindowDelay + WinOffset×1.25 ms). The result becomes a single signed correction we apply to `s_next_hop_time` in T4.

### T3.1 — Offline correlator script

- [ ] **Step 1: Create `python/examples/lab/f8a_session3_offset_analysis.py`**

```python
"""F8A Session 3 — correlate our debug_timing buffer with a Sniffle pcap.

Inputs:
  --json       Output of f8a_session3_capture.py.
  --pcap       Sniffle pcap covering the same connection.
  --target     Peer MAC (default DC:32:62:8D:E1:09).

What it does:
  1. Reads the Sniffle pcap (ETHERTYPE_BLUETOOTH_LL frames, BTLE pseudo-header).
  2. Finds the CONNECT_IND frame addressed to <target>.
     - Extracts: TS (ns), WinOffset (LE u16 at offset 8 of LLData), WinSize.
  3. Computes the peer's first-listen wallclock UNIX ns:
        peer_first_listen = ts(connect_ind_end)
                          + 1.25 ms (transmitWindowDelay)
                          + WinOffset * 1.25 ms
     CONNECT_IND PDU on 1 Mbps takes 304 µs (preamble+AA+header+34 byte PDU
     +CRC) — added to TS to get end-of-frame.
  4. Calibrates Sniffle wall-clock vs FeralRF RAT:
        rat_us(timing[0]) corresponds to wall_unix_ns(start_capture).
     We ALSO know the FW reports st->connTime in conn_status; that is the
     RAT tick at end-of-CONNECT_IND-TX. We map:
        rat_to_wall_ns(connTime) ≈ ts(connect_ind_end_in_pcap)
     yielding a single linear offset: WALL_NS = rat_us*1000 + DELTA.
  5. With DELTA known, projects timing[0].startRAT into wallclock UNIX ns
     and prints:
        master_tx_wall = wall(timing[0].startRAT)
        offset = master_tx_wall - peer_first_listen   (in µs)
     A positive value means we transmit AFTER the peer stops listening;
     a negative value means we transmit BEFORE the peer starts listening.

Sniffle pcap parsing uses scapy if available; the script falls back to a
hand-rolled BTLE record parser if scapy is missing.
"""

import argparse
import json
import struct
import sys
from pathlib import Path


PCAP_GLOBAL_HEADER_LEN = 24
PCAP_RECORD_HEADER_LEN = 16
LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR = 256  # what Sniffle writes


def parse_pcap_records(path: Path):
    """Yield (ts_ns:int, data:bytes) for every record."""
    raw = path.read_bytes()
    if len(raw) < PCAP_GLOBAL_HEADER_LEN:
        raise ValueError("pcap too short")
    magic = struct.unpack("<I", raw[0:4])[0]
    nano = magic == 0xa1b23c4d
    micro = magic == 0xa1b2c3d4
    if not (nano or micro):
        raise ValueError(f"unsupported pcap magic 0x{magic:08x}")
    linktype = struct.unpack("<I", raw[20:24])[0]
    if linktype != LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR:
        print(f"warning: unexpected linktype {linktype}", file=sys.stderr)
    off = PCAP_GLOBAL_HEADER_LEN
    while off + PCAP_RECORD_HEADER_LEN <= len(raw):
        ts_sec, ts_sub, incl, orig = struct.unpack("<IIII", raw[off:off+16])
        ts_ns = ts_sec * 1_000_000_000 + (ts_sub if nano else ts_sub * 1000)
        off += PCAP_RECORD_HEADER_LEN
        data = raw[off:off+incl]
        off += incl
        yield ts_ns, data


def find_connect_ind(records, target_mac_le: bytes):
    """Sniffle BTLE pHdr is 10 bytes; LL PDU follows. CONNECT_IND header
    byte high nibble = 0x05 (PDU type). Return (ts_ns, ll_pdu_bytes)."""
    for ts_ns, data in records:
        if len(data) < 10 + 2 + 12:
            continue
        ll = data[10:]
        if len(ll) < 2:
            continue
        pdu_type = ll[0] & 0x0F
        if pdu_type != 0x05:  # CONNECT_IND
            continue
        # InitA = ll[2:8], AdvA = ll[8:14], LLData = ll[14:36]
        if len(ll) < 14 + 22:
            continue
        adva = ll[8:14]
        if adva == target_mac_le:
            return ts_ns, ll
    return None


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--json", required=True)
    p.add_argument("--pcap", required=True)
    p.add_argument("--target", default="DC:32:62:8D:E1:09")
    args = p.parse_args()

    target_le = bytes(reversed(bytes.fromhex(args.target.replace(":", ""))))
    blob = json.loads(Path(args.json).read_text())
    timing = blob["debug_timing"]
    if not timing:
        print("no debug_timing entries", file=sys.stderr); sys.exit(1)
    conn_time_rat = blob["conn_status"]["conn_time"]
    if conn_time_rat is None:
        print("conn_status.conn_time is None — firmware older than session 1?",
              file=sys.stderr); sys.exit(1)

    records = list(parse_pcap_records(Path(args.pcap)))
    found = find_connect_ind(records, target_le)
    if not found:
        print("no CONNECT_IND in pcap addressed to target", file=sys.stderr); sys.exit(1)
    ts_connect_ind_start_ns, ll = found

    # CONNECT_IND length on 1M = 1+1+6+6+22 + CRC(3) = 39 B over the air,
    # +preamble(1) + AA(4) = 44 B → 44*8 µs = 352 µs.
    connect_ind_air_ns = 352_000
    ts_connect_ind_end_ns = ts_connect_ind_start_ns + connect_ind_air_ns

    # WinOffset is bytes [8..9] of LLData (LLData starts at ll[14]).
    win_offset = struct.unpack("<H", ll[14+8:14+10])[0]
    win_size = ll[14+7]
    interval = struct.unpack("<H", ll[14+10:14+12])[0]
    print(f"CONNECT_IND end ts (ns): {ts_connect_ind_end_ns}")
    print(f"  WinOffset={win_offset} (1.25ms units) "
          f"→ {win_offset*1250} µs")
    print(f"  WinSize={win_size} → {win_size*1250} µs")
    print(f"  Interval={interval} → {interval*1250} µs")

    transmit_window_delay_ns = 1_250_000  # 1.25 ms
    peer_first_listen_ns = (ts_connect_ind_end_ns
                            + transmit_window_delay_ns
                            + win_offset * 1_250_000)
    peer_window_close_ns = peer_first_listen_ns + win_size * 1_250_000
    print(f"peer first-listen open  (ns): {peer_first_listen_ns}")
    print(f"peer first-listen close (ns): {peer_window_close_ns}")

    # Calibrate RAT→wall: connTime is the RAT tick where the Initiator finished
    # CONNECT_IND TX, which equals ts_connect_ind_end_ns.
    rat_us_to_wall_ns = lambda rat: ts_connect_ind_end_ns + (rat - conn_time_rat) * 250
    # 4 MHz RAT → 1 tick = 250 ns.

    print(f"connTime (RAT) = {conn_time_rat}")
    print(f"  → wall ns     = {rat_us_to_wall_ns(conn_time_rat)} (anchor)")

    e0 = timing[0]
    master0_start_wall = rat_us_to_wall_ns(e0["start_rat"])
    offset_us = (master0_start_wall - peer_first_listen_ns) // 1000
    print(f"\nfirst master event:")
    print(f"  startRAT = {e0['start_rat']}, endRAT = {e0['end_rat']}")
    print(f"  → start_wall_ns = {master0_start_wall}")
    print(f"  → end_wall_ns   = {rat_us_to_wall_ns(e0['end_rat'])}")
    print(f"  → status        = 0x{e0['status']:04X}")
    print(f"  offset master_tx − peer_listen_open = {offset_us} µs")
    if offset_us > 0:
        print(f"  → MASTER STARTS {offset_us} µs AFTER peer opens "
              "(decrease nextHopTime by ~that many ticks)")
    else:
        print(f"  → MASTER STARTS {-offset_us} µs BEFORE peer opens "
              "(increase nextHopTime by ~that many ticks)")

    delta_ticks = (master0_start_wall - peer_first_listen_ns) // 250
    print(f"  delta in RAT ticks = {delta_ticks}")
    print(f"\nProposed fix: replace `+ s_hop_interval_ticks` first-anchor "
          "calculation with `+ s_hop_interval_ticks - ({delta} ticks)` "
          "in BleConnMgr_start.".format(delta=delta_ticks))


if __name__ == "__main__":
    main()
```

### T3.2 — Run the analysis

- [ ] **Step 1: Run the script**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && python3 examples/lab/f8a_session3_offset_analysis.py \
  --json /home/sabas/Documents/electroniccats/FeralRF/docs/investigations/2026-04-24-f8a-session-1/session-3-capture-fixed.json \
  --pcap /home/sabas/Documents/electroniccats/FeralRF/docs/investigations/2026-04-24-f8a-session-1/session-3-sniffle.pcap \
  --target DC:32:62:8D:E1:09 | tee /tmp/f8a-s3-offset.txt
```
Expected: prints WinOffset / Interval, peer first-listen window, and a `offset master_tx − peer_listen_open = <N> µs` value with a proposed `delta_ticks`.

- [ ] **Step 2: Sanity check**

The proposed `delta_ticks` MUST be:
- A small fraction of `interval × 4000` (e.g. < interval). For 30 ms interval the bound is `< 120000` ticks.
- Nonzero. If it is exactly zero, our timing is already correct and the bug lies elsewhere — **STOP and write a session-3-debug-note.md** before proceeding to T4. Do not blindly tweak.

If the bound check fails, the calibration is wrong. The most likely cause is a Sniffle-side discrepancy in `linktype`/pHdr layout. Re-read the script's `parse_pcap_records` against the actual pcap header (`xxd -l 24 session-3-sniffle.pcap`) and adjust before continuing.

### T3.3 — Commit T2+T3 artifacts (no firmware change yet)

- [ ] **Step 1: Pre-commit**

Run: `cd /home/sabas/Documents/electroniccats/FeralRF && pre-commit run --files python/examples/lab/f8a_session3_capture.py python/examples/lab/f8a_session3_offset_analysis.py`
Expected: PASS.

- [ ] **Step 2: Commit**

```bash
git add python/examples/lab/f8a_session3_capture.py \
        python/examples/lab/f8a_session3_offset_analysis.py \
        docs/investigations/2026-04-24-f8a-session-1/session-3-capture-fixed.json \
        docs/investigations/2026-04-24-f8a-session-1/session-3-sniffle.pcap
git commit -m "telemetry(f8a): paired FeralRF/Sniffle capture + offset analyzer

Captures the JSON dump from CMD_DEBUG_TIMING + CMD_CONN_STATUS alongside
a Sniffle pcap of the same connection. The analyzer projects our master
event start RAT into the pcap wallclock and reports the (signed) µs
offset between our first master TX and the peer's first listening
window. Used to drive Session 3's anchor fix."
```

---

## Task 4 — Apply the timing fix and validate

Goal: Apply the single-constant correction from T3 and demonstrate (a) sustained connection, (b) no PHY regression.

### T4.1 — Apply the fix

- [ ] **Step 1: Edit `firmware/cc1352/src/ble_conn_mgr.c`**

In `BleConnMgr_start`, find the line:

```c
    s_next_hop_time = st->connTime - AO_TARG + s_hop_interval_ticks;
```

Replace it with the corrected expression. Substitute `<DELTA>` with the integer `delta_ticks` value printed by T3.2 (signed; positive = subtract from anchor, since master_tx was AFTER peer_listen):

```c
    /* Anchor correction from F8A Session 3 telemetry: aligns master TX
     * with peer first-listen window (CH573 oracle). See
     * docs/investigations/2026-04-24-f8a-session-1/session-3-closeout.md. */
    #define BLE_CONN_MGR_ANCHOR_CAL_TICKS (<DELTA>)
    s_next_hop_time = st->connTime - AO_TARG + s_hop_interval_ticks
                      - BLE_CONN_MGR_ANCHOR_CAL_TICKS;
```

If the analyzer reported negative `delta_ticks` (master_tx BEFORE peer_listen), the same expression is correct: subtracting a negative value adds.

- [ ] **Step 2: Build**

Run: `cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -10`
Expected: clean build.

- [ ] **Step 3: Flash board #1**

Run: `python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py flash -d 1 /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex`
Expected: success (retry up to 2×).

### T4.2 — Sustained-connection probe

- [ ] **Step 1: Quick sanity (10 s linger)**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && python3 examples/lab/f8a_session3_capture.py \
  --linger 10.0 \
  --out /tmp/f8a-s3-postfix-10s.json
python3 -c "
import json; d = json.load(open('/tmp/f8a-s3-postfix-10s.json'))
s = d['conn_status']
print('connected=', s['connected'], 'events=', s['events'],
      'last_status=0x%04X' % s['last_status'],
      'tx_done=', s['tx_done'], 'total_rx=', s['total_rx'])
"
```
Expected: `connected= True last_status=0x1400` and `total_rx > 0` (BLE_DONE_OK, not 0x1402, with at least one received PDU).

If still 0x1402: re-run T3 with the new pcap+JSON (fresh `delta_ticks`). The first iteration may not be tight enough — but DO NOT iterate more than 2× without escalating. After 2 unsuccessful corrections, write the failure to `session-3-closeout.md` and STOP.

- [ ] **Step 2: 60 s soak**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && timeout 75 python3 -c "
from feralrf import Radio
import time
r = Radio('/dev/ttyACM0'); r.connect(); r.init()
res = r.ble_connect('DC:32:62:8D:E1:09', addr_type=0)
print('connect:', res.result)
deadline = time.time() + 60
while time.time() < deadline:
    s = r.conn_status()
    print(time.strftime('%H:%M:%S'),
          'connected=', s.connected,
          'events=', s.events, 'last_status=0x%04X' % s.last_status,
          'tx_done=', s.tx_done, 'total_rx=', s.total_rx)
    if not s.connected: break
    time.sleep(5)
r.ble_disconnect(); r.close()
"
```
Expected: `connected=True` for the entire 60 s, `events` strictly increasing (~33 / 5 s at 30 ms interval), `last_status=0x1400` more often than not, `total_rx > 0`.

### T4.3 — 8/8 PHY regression

- [ ] **Step 1: Run validation matrix**

Run: `cd /home/sabas/Documents/electroniccats/FeralRF/python && pytest tests/ -v -m "not hardware" 2>&1 | tail -30`
Expected: all collected tests PASS (Python contract layer is the regression gate; `-m hardware` tests are operator-driven and not required here).

- [ ] **Step 2: OTA spot-check on 4 PHYs**

For each `(phy, channel, freq)` in `[(BLE_1M, 37, 2402_000_000), (IEEE_802_15_4, 11, 2405_000_000), (SUB_1GHZ_868, 0, 868_000_000), (PROPRIETARY_GFSK, 0, 433_920_000)]`:

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && timeout 8 python3 -c "
from feralrf import Radio; from feralrf.enums import PHY
import time
r = Radio('/dev/ttyACM0'); r.connect(); r.init()
r.set_phy(PHY.<NAME>, <CH>, <FREQ>); r.start_rx()
time.sleep(3); n = sum(1 for _ in r.read_packets()); r.stop_rx()
print('<NAME>: rx=', n)
r.close()
"
```
Expected: `rx > 0` for BLE_1M (CH573 visible) and IEEE_802_15_4 (any 802.15.4 traffic), `rx ≥ 0` for sub-1GHz / PROPRIETARY_GFSK (may be quiet — log RSSI floor is sufficient; 0 is acceptable but firmware must not crash).

If the firmware crashes or any PHY hangs: revert the T4.1 edit (`git diff firmware/cc1352/src/ble_conn_mgr.c`) and STOP.

### T4.4 — Commit the fix

- [ ] **Step 1: Pre-commit**

Run: `pre-commit run --files firmware/cc1352/src/ble_conn_mgr.c`

- [ ] **Step 2: Commit**

```bash
git add firmware/cc1352/src/ble_conn_mgr.c
git commit -m "fix(f8a): align master anchor with peer listen window

Session 3 telemetry (RSP_DEBUG_TIMING + Sniffle pcap correlation)
showed our first master TX missed CH573's first listen window by
<DELTA> RAT ticks. Subtracting that constant from the initial
nextHopTime brings the anchor into spec; subsequent events stay
aligned because s_next_hop_time advances by the same hopInterval
each event. With this fix, 60 s sustained connection holds and
last_status returns BLE_DONE_OK (0x1400) instead of NOSYNC
(0x1402). Reference: session-3-closeout.md."
```

---

## Task 5 — GATT discover + read end-to-end

Goal: With a sustained connection, drive GATT discovery and read at least one characteristic (Device Name, UUID 0x2A00) on CH573, closing F8 inline.

### T5.1 — GATT discover

- [ ] **Step 1: Run discovery**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && timeout 30 python3 -c "
from feralrf import Radio
r = Radio('/dev/ttyACM0'); r.connect(); r.init()
res = r.ble_connect('DC:32:62:8D:E1:09', addr_type=0)
print('connect:', res.result)
disc = r.gatt_discover(timeout=20.0)
print(f'services={len(disc.services)} chars={len(disc.characteristics)}')
for s in disc.services:
    print('  svc', f'0x{s.start_handle:04X}-0x{s.end_handle:04X}', 'uuid', s.uuid.hex())
for c in disc.characteristics[:8]:
    print('  chr handle=0x{:04X} value=0x{:04X} uuid={}'.format(
        c.handle, c.value_handle, c.uuid.hex()))
r.ble_disconnect(); r.close()
"
```
Expected: `services ≥ 1`, `chars ≥ 1`. Device Name characteristic UUID `00 2A` (little-endian) appears.

### T5.2 — GATT read Device Name

- [ ] **Step 1: Identify the Device Name value handle**

From T5.1 output, find a characteristic with UUID `00 2A` (16-bit Device Name) and note its `value_handle`. Call it `<H>`.

- [ ] **Step 2: Read it**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && timeout 20 python3 -c "
from feralrf import Radio
r = Radio('/dev/ttyACM0'); r.connect(); r.init()
r.ble_connect('DC:32:62:8D:E1:09', addr_type=0)
val = r.gatt_read(<H>, timeout=10.0)
print('handle=0x{:04X} bytes={} ascii={!r}'.format(val.handle, val.data.hex(), val.data.decode(errors='replace')))
r.ble_disconnect(); r.close()
"
```
Expected: `bytes` non-empty; `ascii` is the device name string (e.g. `'CH573 BLE'` or similar).

### T5.3 — Reconnect cleanliness

- [ ] **Step 1: Connect → disconnect → connect (no reset)**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && timeout 30 python3 -c "
from feralrf import Radio
r = Radio('/dev/ttyACM0'); r.connect(); r.init()
for i in range(2):
    res = r.ble_connect('DC:32:62:8D:E1:09', addr_type=0)
    s = r.conn_status()
    print(f'iter {i}: result={res.result} connected={s.connected} events={s.events}')
    r.ble_disconnect()
r.close()
"
```
Expected: both iterations succeed (`connected=True`, `events>0`).

### T5.4 — Commit GATT validation evidence

- [ ] **Step 1: Save the GATT log**

Capture the output of T5.1 + T5.2 + T5.3 into `docs/investigations/2026-04-24-f8a-session-1/session-3-gatt-validation.txt`.

- [ ] **Step 2: Pre-commit + commit**

```bash
git add docs/investigations/2026-04-24-f8a-session-1/session-3-gatt-validation.txt
git commit -m "docs(f8a): GATT discover+read evidence on CH573 (closes F8)

Session 3 proves GATT round-trip end-to-end: discovery returns the
peripheral's services and characteristics, gatt_read on Device Name
returns ASCII bytes, and reconnect-without-reset works twice in a
row. F8A's blocker was the master anchor timing; F8's blocker was
F8A. Both close together."
```

---

## Task 6 — ICall cleanup + tag `v2.0-f8a`

Goal: Remove the BLE5-Stack ICall residue (decision #18 in plan v2 — we explicitly chose Sniffle-style raw RF over ICall), confirm the build still passes, then tag the branch.

### T6.1 — Drop ICall calls from `main_rtos.c`

- [ ] **Step 1: Locate the block**

Run: `grep -n "ICall_\|appServiceInfo" /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/src/main_rtos.c`
Expected: prints lines 216-221 region.

- [ ] **Step 2: Edit `firmware/cc1352/src/main_rtos.c`**

Find lines 216-221 (the block beginning `/* BLE5-Stack ICall — disabled until Phase M3 */`). Delete the entire block including the comment, the `appServiceInfo->timerMaxMillisecond = ICall_getMaxMSecs();`, the `ICall_init();`, and the `ICall_createRemoteTasks();`. Replace them with a single comment:

```c
    /* No ICall: F8A uses Sniffle-style raw RF central; BLE5-Stack disabled. */
```

If the file also has an `#include` for ICall headers that becomes unused after this delete, remove that include too.

### T6.2 — Delete ICall residue files

- [ ] **Step 1: Delete the unused source files**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git rm firmware/cc1352/startup/osal_icall_ble.c \
       firmware/cc1352/syscfg/ti_ble_config.c \
       firmware/cc1352/syscfg/ti_ble_config.h
```
Expected: three files removed from the index.

- [ ] **Step 2: Update CMakeLists.txt**

Run: `grep -n "osal_icall_ble\|ti_ble_config" /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/CMakeLists.txt`
Expected: a list of source-file references.

For each match, edit `firmware/cc1352/CMakeLists.txt` to remove that path from the source list. If the deletion leaves an empty `set()` block, remove the empty block too.

- [ ] **Step 3: Drop now-unused stubs**

Run: `grep -n "ICall_" /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/src/rtos_stubs.c`
If the file still references `ICall_getMaxMSecs` and that symbol is no longer linked, remove the stub from `rtos_stubs.c`. If a build error in T6.3 reveals it's still needed, restore it.

### T6.3 — Rebuild + sanity test

- [ ] **Step 1: Clean rebuild**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352 && rm -rf build && mkdir build && cd build && cmake .. 2>&1 | tail -10 && make -j$(nproc) 2>&1 | tail -10
```
Expected: clean cmake config, clean build, `feralrf_cc1352.hex` regenerated, no warnings.

- [ ] **Step 2: Flash board #1**

Run: `python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py flash -d 1 /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex`
Expected: success.

- [ ] **Step 3: Smoke test — connection still works post-cleanup**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python && timeout 20 python3 -c "
from feralrf import Radio
r = Radio('/dev/ttyACM0'); r.connect(); r.init()
res = r.ble_connect('DC:32:62:8D:E1:09', addr_type=0)
import time; time.sleep(5)
s = r.conn_status()
print('connected=', s.connected, 'events=', s.events,
      'last_status=0x%04X' % s.last_status, 'total_rx=', s.total_rx)
r.ble_disconnect(); r.close()
"
```
Expected: same `connected=True last_status=0x1400` profile as T4.2 step 2.

### T6.4 — Closeout doc + commit + tag

- [ ] **Step 1: Write `session-3-closeout.md`**

Create `docs/investigations/2026-04-24-f8a-session-1/session-3-closeout.md` mirroring the format of `session-2-closeout.md`. Sections (in order):

```markdown
# F8A Session 3 — close-out report

**Date:** 2026-04-25
**Branch:** `feature/f8a-ble-central-sniffle`
**Range from Session 2:** `fcb016f..HEAD`

## What landed

- `CMD_DEBUG_TIMING` / `RSP_DEBUG_TIMING` (commit hash from T1.10).
- Paired capture artifacts: `session-3-capture-fixed.json`, `session-3-sniffle.pcap`.
- Anchor calibration constant `BLE_CONN_MGR_ANCHOR_CAL_TICKS = <DELTA>` (commit hash from T4.4).
- GATT discover+read evidence: `session-3-gatt-validation.txt` (commit hash from T5.4).
- ICall residue removal (commit hash from T6.4 itself, after this doc lands).

## What the telemetry told us

(short paragraph — paste the relevant lines from `/tmp/f8a-s3-offset.txt`).

## Validation

- 60 s sustained connection: `connected=True` throughout, `last_status=0x1400` ≥ 80 % of polls, `total_rx > 0`.
- 8/8 PHY regression: BLE_1M / IEEE_802_15_4 / SUB_1GHZ_868 / PROPRIETARY_GFSK responsive after the fix; full pytest suite green.
- GATT: discovery returns ≥1 svc and ≥1 chr, Device Name read returns ASCII bytes, two consecutive connect/disconnect cycles work without reset.

## What remains for v2.0-f9 and later

(carry over from plan v2 §5: F9 is the 868→BLE PHY switch fix, F10 is the props port, F11 is the BLE attacks port. Scanner / spectrum / jamming come later.)
```

- [ ] **Step 2: Pre-commit + commit cleanup**

Run: `pre-commit run --files firmware/cc1352/src/main_rtos.c firmware/cc1352/CMakeLists.txt firmware/cc1352/src/rtos_stubs.c docs/investigations/2026-04-24-f8a-session-1/session-3-closeout.md`

```bash
git add firmware/cc1352/src/main_rtos.c \
        firmware/cc1352/CMakeLists.txt \
        firmware/cc1352/src/rtos_stubs.c \
        docs/investigations/2026-04-24-f8a-session-1/session-3-closeout.md
git commit -m "chore(f8a): drop ICall/BLE5-Stack residue + Session 3 closeout

Decision #18 of the v2 plan picked Sniffle-style raw RF over ICall
for GATT. With F8A green via raw RF, the dormant ICall scaffolding
is dead weight and gets removed:
  - osal_icall_ble.c, ti_ble_config.{c,h}
  - ICall_init / ICall_createRemoteTasks calls in main_rtos.c
  - now-unused rtos_stubs.c entries
The Session 3 closeout summarises what landed, what the telemetry
showed, and what's next (F9 868→BLE switch, F10 props port).
"
```

- [ ] **Step 3: Tag `v2.0-f8a`**

```bash
git tag -a v2.0-f8a -m "F8A — BLE central via raw RF (Sniffle-style), GATT round-trip green.

CONNECT_IND with valid static-random InitA (Session 2),
master anchor calibrated against Sniffle pcap oracle (Session 3),
GATT discover+read on CH573, ICall residue removed.
F8 closes inline."
git tag --list 'v2.0*'
```
Expected: `v2.0-f8a` listed.

- [ ] **Step 4: Update memory index**

Add a one-line entry to `/home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md` under `## Project`:

```
- [project_f8a_close.md](project_f8a_close.md) — F8A closed v2.0-f8a: raw-RF central, GATT live, ICall purged
```

Then create `/home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f8a_close.md` with frontmatter (`type: project`) and a 5-line body summarising the close: the calibration constant, the GATT evidence, and the next phase (F9). Why: future sessions will need to know that ICall is intentionally gone before they hit a "missing ICall_init" error and try to add it back.

---

## Self-review log

- **Spec coverage** — every entry from the user's "Estructura sugerida" is mapped: T1 ↔ RSP_DEBUG_TIMING fw+Python; T2 ↔ paired capture; T3 ↔ offset calc + fix decision; T4 ↔ apply fix + 60 s + 8/8 regression; T5 ↔ GATT discover+read; T6 ↔ ICall cleanup + tag.
- **Constraints honored** — H1/H2/H3 not retried; CMD_BLE5_GENERIC_TX not invoked; `bDynamicWinOffset=1` and the Sniffle anchor formula preserved unless T3 says otherwise.
- **Type consistency** — `DebugTimingEntry` / `DebugTimingResponse` referenced identically across `_responses.py`, `radio.py`, `tests/test_debug_timing.py`. C-side `BleConnMgr_DbgTimingEntry` 11-byte logical layout serialises to the same 13-byte wire layout (u16+u32+u32+u16+u8) used by the parser.
- **Placeholder scan** — `<DELTA>` and `<H>` are explicitly defined as values produced by an earlier task (T3.2 and T5.1 respectively). No "TBD"/"TODO"/"add appropriate error handling" survives.
- **Risk: T3 calibration assumption** — the analyzer assumes Sniffle's pcap timestamps are wall-clock ns and that `connTime` in `conn_status` is the RAT tick at end-of-CONNECT_IND-TX. Both are independently documented (Session 1 closeout wrote the connTime claim; Sniffle pcap timestamps follow standard pcap semantics). If either turns out wrong, T3.2 step 2 has an explicit STOP-and-debug exit.

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-04-25-f8a-session-3-telemetry-and-close.md`. Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration.
2. **Inline Execution** — Execute tasks in this session using `superpowers:executing-plans`, batch execution with checkpoints.

Which approach?
