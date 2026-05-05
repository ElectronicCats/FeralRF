# F20.a.1.b NOSYNC Debug Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add slave-side telemetry (`CMD_DEBUG_SLAVE` 0x49 → `RSP_DEBUG_SLAVE` 0xAA) so the host can diff parsed CONNECT_IND values against the central's actuals AND inspect per-event slave RX behavior, then iterate fixes until smoke V2 passes (field match + nRxOk > 0 + GATT path works).

**Architecture:** New debug command mirrors the F8A `CMD_DEBUG_TIMING` / `CMD_DEBUG_CONN_PARAMS` ring-buffer pattern. `BleConnMgr_startSlave` snapshots the parsed params + first anchor; `BleConnMgr_pollSlave` appends a 17-byte ring entry per event. Smoke V2 harness queries both boards on disconnect, diffs fields, prints the ring summary, and asserts pass criteria.

**Tech Stack:** CC1352P7 firmware (TI-RTOS 7, SDK 8.30, CMake), Python 3.11+ (pyserial, pytest), pre-commit.

**Spec source:** `docs/superpowers/specs/2026-05-04-f20a1b-nosync-debug-design.md` (commit `7379325`).

**Branch:** `feature/f20a1-peripheral-read` continues from HEAD `7379325` (post tag `v2.0-f20.a.1-partial`). NO new branch — partial tag is a checkpoint, not a hard close.

**Hardware:** Two CatSniffers (peripheral + central). Re-detect ports each session — USB enumeration shifts. Use `python -m catnip devices` from `~/Documents/electroniccats/CatSniffer-Tools/catnip/`.

**Wire format constraint:** `PROTOCOL_MAX_PAYLOAD = 255`. RSP_DEBUG_SLAVE = 26 B header + DEPTH × 17 B entries. With DEPTH = 13, total = 26 + 221 = **247 B** ≤ 255. (1 second window at 30ms interval = ~33 events; ring keeps the most recent 13 — sufficient for the tail before DC.)

**Prerequisites verified at planning time (2026-05-04):**
- ✅ F8A telemetry pattern: `s_dbg_timing[14]` ring + `s_dbg_timing_head` + `s_dbg_timing_count` + `BleConnMgr_getDebugTiming(out, max)` accessor (in `firmware/cc1352/src/ble_conn_mgr.c:99-101,530-545`). Ring saturates at depth, count saturates at depth, accessor walks oldest-first.
- ✅ `BleConnMgr_SlaveParams` typedef contains plain values only — `uint32_t accessAddr, crcInit, connectIndEndRat; uint16_t hopInterval_125us, latency, supervTimeout_10ms, winOffset_125us; uint8_t hopIncrement` (in `firmware/cc1352/include/ble_conn_mgr.h:86-96`). Memcpy-safe.
- ✅ Existing `CMD_DEBUG_CONN_PARAMS` (0x48) handler in `command_processor.c:1092-1145` returns 50 B; layout known. Smoke V2 will call it on the central side.
- ✅ Free opcodes: 0x49 (next after 0x48) and 0xAA (in 0xA0-0xAF response range, between RSP_GATT_NOTIFY 0xA6 and RSP_DEBUG_TIMING 0xA8). Verified via `grep -n "0x49\|0xAA" firmware/cc1352/include/protocol.h python/feralrf/enums.py`.
- ✅ `BleConnMgr_pollSlave` is the right population point — it returns the slave's `stats` struct (nRxOk/nRxNok/nRxIgnored/pktStatus) plus access to `s_slave_event_counter` and the locally computed `chan`.

---

## Task 0: Re-confirm baseline + sync working tree

**Files:** None (git operation)

- [ ] **Step 1: Confirm branch state**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git status
git log --oneline -5
```

Expected:
```
7379325 spec(f20.a.1.b): NOSYNC debug via slave telemetry — design
2060967 fix(f20.a.1): NOSYNC mitigation — HW timestamp + slave anchor catch-up
7e2f520 test(f20.a.1): smoke V1 cross-validation 2-board + nRF Connect demo lab
...
```

Working tree clean, branch `feature/f20a1-peripheral-read`.

- [ ] **Step 2: Confirm partial tag exists**

```bash
git tag --list "v2.0-f20*"
```

Expected: `v2.0-f20.a.1-partial`. If missing, do not proceed — re-tag from prior session.

---

## Task 1: Firmware — protocol IDs + ring buffer + populate hooks

**Files:**
- Modify: `firmware/cc1352/include/protocol.h`
- Modify: `firmware/cc1352/include/ble_conn_mgr.h`
- Modify: `firmware/cc1352/src/ble_conn_mgr.c`

- [ ] **Step 1: Add `CMD_DEBUG_SLAVE` and `RSP_DEBUG_SLAVE` to `protocol.h`**

Find the F8A debug IDs (existing):

```bash
grep -n "CMD_DEBUG_TIMING\|RSP_DEBUG_TIMING\|CMD_DEBUG_CONN_PARAMS\|RSP_DEBUG_CONN_PARAMS" firmware/cc1352/include/protocol.h
```

Add immediately after `CMD_DEBUG_CONN_PARAMS 0x48u`:

```c
#define CMD_DEBUG_SLAVE 0x49u /* F20.a.1.b */
```

Add immediately after `RSP_DEBUG_CONN_PARAMS 0xA9u`:

```c
#define RSP_DEBUG_SLAVE 0xAAu /* F20.a.1.b */
```

Match existing comment style (most existing IDs have a phase tag in the comment).

- [ ] **Step 2: Extend `ble_conn_mgr.h` with slave-debug types + accessors**

After the existing `BleConnMgr_DbgTimingEntry` typedef and `BleConnMgr_getDebugTiming` declaration, but BEFORE the `BleConnMgr_SlaveParams` block, insert:

```c
/* F20.a.1.b — slave-side per-event ring entry. 17 wire bytes; depth 13 keeps
 * the response payload (26 B header + 13*17 B = 247 B) ≤ PROTOCOL_MAX_PAYLOAD. */
#define BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH 13u

typedef struct {
    uint16_t event_counter;
    uint8_t chan;
    uint32_t anchor_rat;        /* Computed anchor for this event (post catch-up). */
    uint32_t actual_start_rat;  /* RF_getCurrentTime() right before RadioIF_bleSlave. */
    uint16_t status;            /* Ble5_0_cmdBle5Slave.status raw value. */
    uint8_t nRxOk;
    uint8_t nRxNok;
    uint8_t nRxIgnored;
    uint8_t pktStatus;          /* Packed bitfield from output.pktStatus. */
} BleConnMgr_DbgSlaveEntry;

/* Snapshot of the slave params seen by BleConnMgr_startSlave + the first
 * anchor it computed. Populated once per slave session start. The
 * connectIndEndRat / winOffset_125us etc. fields come from the raw
 * BleConnMgr_SlaveParams struct, which is itself plain values. */
void BleConnMgr_getDbgSlaveSnapshot(BleConnMgr_SlaveParams *out_params,
                                    uint32_t *out_first_anchor_rat);

/* Returns up to maxEntries snapshots of the most recent slave events,
 * oldest first. The returned count = min(active entries, maxEntries). */
uint8_t BleConnMgr_getDbgSlaveRing(BleConnMgr_DbgSlaveEntry *out_buf, uint8_t max_entries);
```

- [ ] **Step 3: Add static state to `ble_conn_mgr.c`**

Find the existing slave state block:

```bash
grep -n "s_slave_running\|s_slave_anchor_rat\|F20.a.1 — Slave" firmware/cc1352/src/ble_conn_mgr.c | head -5
```

Insert AFTER `static uint32_t s_slave_last_rx_rat = 0u;`:

```c
/* F20.a.1.b — slave debug ring (separate from F8A central's s_dbg_timing). */
static BleConnMgr_DbgSlaveEntry s_dbg_slave_ring[BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH];
static uint8_t s_dbg_slave_head;
static uint8_t s_dbg_slave_count;
static BleConnMgr_SlaveParams s_dbg_slave_params_snapshot;
static uint32_t s_dbg_slave_first_anchor;
```

- [ ] **Step 4: Capture snapshot in `BleConnMgr_startSlave`**

Find the function (around line 548):

```bash
grep -n "^void BleConnMgr_startSlave\b" firmware/cc1352/src/ble_conn_mgr.c
```

Read the function body. After `s_slave_params = *params;` and the `s_slave_anchor_rat = ...` block, insert:

```c
    /* F20.a.1.b — debug snapshot (overwrites previous session). */
    s_dbg_slave_params_snapshot = *params;
    s_dbg_slave_first_anchor = s_slave_anchor_rat;
    s_dbg_slave_head = 0u;
    s_dbg_slave_count = 0u;
```

- [ ] **Step 5: Append entry in `BleConnMgr_pollSlave`**

Find the function (around line 577):

```bash
grep -n "^bool BleConnMgr_pollSlave\b" firmware/cc1352/src/ble_conn_mgr.c
```

Read the function body — particularly the section that calls `RadioIF_bleSlave(...)` and captures `numSent` / `stats`. Just BEFORE the call to `RadioIF_bleSlave`, capture the actual start time:

```c
    uint32_t actual_start = RF_getCurrentTime();
```

After the `RadioIF_bleSlave(...)` call returns and AFTER `TXQueue_flush((uint8_t)numSent)` AND AFTER `RadioIF_bleDrainRxQueue()` AND AFTER the `if (stats.nRxOk > 0u) { s_slave_last_rx_rat = ...; }` block, but BEFORE the `Ble20_drainAndDispatch(&reason)` call, insert:

```c
    /* F20.a.1.b — append slave event to debug ring. */
    {
        BleConnMgr_DbgSlaveEntry *e = &s_dbg_slave_ring[s_dbg_slave_head];
        e->event_counter = s_slave_event_counter;
        e->chan = chan;
        e->anchor_rat = s_slave_anchor_rat;
        e->actual_start_rat = actual_start;
        e->status = (uint16_t)status;
        e->nRxOk = stats.nRxOk;
        e->nRxNok = stats.nRxNok;
        e->nRxIgnored = stats.nRxIgnored;
        e->pktStatus = stats.pktStatus;
        s_dbg_slave_head = (uint8_t)((s_dbg_slave_head + 1u) % BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH);
        if (s_dbg_slave_count < BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH) {
            s_dbg_slave_count++;
        }
    }
```

The `int status = RadioIF_bleSlave(...)` local already exists in the body — verify by grepping:

```bash
grep -n "int status = RadioIF_bleSlave" firmware/cc1352/src/ble_conn_mgr.c
```

If the variable is named differently in this revision (`int rc`, etc.), use the actual name.

- [ ] **Step 6: Implement accessors at end of `ble_conn_mgr.c`**

Find the existing `BleConnMgr_getDebugTiming` function for reference (around line 530). Add the two new accessors just BEFORE the slave-running functions OR after `BleConnMgr_getDebugTiming` — match the file's existing accessor grouping.

```c
void BleConnMgr_getDbgSlaveSnapshot(BleConnMgr_SlaveParams *out_params,
                                    uint32_t *out_first_anchor_rat) {
    if (out_params != NULL) {
        *out_params = s_dbg_slave_params_snapshot;
    }
    if (out_first_anchor_rat != NULL) {
        *out_first_anchor_rat = s_dbg_slave_first_anchor;
    }
}

uint8_t BleConnMgr_getDbgSlaveRing(BleConnMgr_DbgSlaveEntry *out_buf, uint8_t max_entries) {
    if (out_buf == NULL || max_entries == 0u) {
        return 0u;
    }
    uint8_t n = (s_dbg_slave_count < max_entries) ? s_dbg_slave_count : max_entries;
    /* Walk oldest-first: start = (head + DEPTH - count) mod DEPTH */
    uint8_t start = (uint8_t)((BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH + s_dbg_slave_head -
                               s_dbg_slave_count) %
                              BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH);
    for (uint8_t i = 0u; i < n; i++) {
        out_buf[i] = s_dbg_slave_ring[(start + i) % BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH];
    }
    return n;
}
```

- [ ] **Step 7: Build**

```bash
cmake --build firmware/cc1352/build -j2 2>&1 | tail -10
```

Expected: clean. The new code is statically allocated; no .bss inflation issue.

- [ ] **Step 8: Pre-commit + commit Bundle 1**

```bash
pre-commit run --files firmware/cc1352/include/protocol.h firmware/cc1352/include/ble_conn_mgr.h firmware/cc1352/src/ble_conn_mgr.c
git add firmware/cc1352/include/protocol.h firmware/cc1352/include/ble_conn_mgr.h firmware/cc1352/src/ble_conn_mgr.c
git commit -m "$(cat <<'EOF'
feat(f20.a.1.b): slave-side debug ring + accessors

Adds CMD_DEBUG_SLAVE / RSP_DEBUG_SLAVE protocol IDs and the firmware
state needed for the host-visible slave diagnostic dump:
- BleConnMgr_DbgSlaveEntry typedef + DEPTH=13 ring buffer
- BleConnMgr_getDbgSlaveSnapshot / BleConnMgr_getDbgSlaveRing accessors
- BleConnMgr_startSlave snapshots params + first anchor
- BleConnMgr_pollSlave appends per-event entry (event_counter, chan,
  anchor_rat, actual_start_rat, status, nRxOk, nRxNok, nRxIgnored,
  pktStatus)

No host wiring yet — Bundle 2 adds the command_processor handler.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

If pre-commit auto-fixes, re-stage and commit. Do NOT use `--no-verify`.

---

## Task 2: Firmware — `command_processor` handler for `CMD_DEBUG_SLAVE`

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c`

- [ ] **Step 1: Find the existing `CMD_DEBUG_CONN_PARAMS` case as reference**

```bash
grep -n "case CMD_DEBUG_CONN_PARAMS\|case CMD_DEBUG_TIMING" firmware/cc1352/src/command_processor.c
```

Read it (around line 1092). Note how it uses `BleConn_getState()`, packs bytes via index assignments, and calls `send_response(RSP_DEBUG_CONN_PARAMS, seq, rsp, 50u)`. The new handler follows the same shape.

- [ ] **Step 2: Add the new case**

Insert AFTER the `case CMD_DEBUG_CONN_PARAMS` block:

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

        /* Wire layout (26 B header + n * 17 B entries):
         *   accessAddr        u32 LE   (4)
         *   crcInit           u32 LE   (4)
         *   winOffset_125us   u16 LE   (2)
         *   hopInterval_125us u16 LE   (2)
         *   latency           u16 LE   (2)
         *   supervTimeout_10ms u16 LE  (2)
         *   hopIncrement      u8       (1)
         *   connectIndEndRat  u32 LE   (4)
         *   firstAnchorRat    u32 LE   (4)
         *   count             u8       (1)
         *   entries[n] each:
         *     event_counter   u16 LE   (2)
         *     chan            u8       (1)
         *     anchor_rat      u32 LE   (4)
         *     actual_start    u32 LE   (4)
         *     status          u16 LE   (2)
         *     nRxOk           u8       (1)
         *     nRxNok          u8       (1)
         *     nRxIgnored      u8       (1)
         *     pktStatus       u8       (1) */
        uint8_t rsp[26u + BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH * 17u];
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
        rsp[25] = n;

        for (uint8_t i = 0u; i < n; i++) {
            uint8_t *p = &rsp[26u + (uint16_t)i * 17u];
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

        send_response(RSP_DEBUG_SLAVE, seq, rsp, (uint16_t)(26u + (uint16_t)n * 17u));
        return;
    }
```

Verify the file already includes `ble_conn_mgr.h` — `grep -n '#include "ble_conn_mgr.h"' firmware/cc1352/src/command_processor.c`. If not (it should — F20.a.1 wired it in), add it near the other includes.

- [ ] **Step 2.1: Build**

```bash
cmake --build firmware/cc1352/build -j2 2>&1 | tail -10
```

Expected: clean. Watch for any `BleConnMgr_DbgSlaveEntry` undefined or `BLE_CONN_MGR_DBG_SLAVE_RING_DEPTH` missing — that means Task 1's header changes didn't land properly.

- [ ] **Step 3: Pre-commit + commit Bundle 2**

```bash
pre-commit run --files firmware/cc1352/src/command_processor.c
git add firmware/cc1352/src/command_processor.c
git commit -m "$(cat <<'EOF'
feat(f20.a.1.b): CMD_DEBUG_SLAVE handler

Returns 26 B header (parsed CONNECT_IND snapshot + computed first
anchor) + n × 17 B ring entries in RSP_DEBUG_SLAVE. Wire layout
documented inline; mirrors CMD_DEBUG_CONN_PARAMS conventions so the
host can diff the two side-by-side.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: Python — enums + `Radio.debug_slave()` + `Radio.debug_conn_params()`

**Files:**
- Modify: `python/feralrf/enums.py`
- Modify: `python/feralrf/radio.py`

- [ ] **Step 1: Add `Command.DEBUG_SLAVE` and `Response.DEBUG_SLAVE` to enums**

Find the existing `# Diagnostics` section in `python/feralrf/enums.py`:

```bash
grep -n "DEBUG_TIMING\|DEBUG_CONN_PARAMS" python/feralrf/enums.py
```

Add immediately after `DEBUG_CONN_PARAMS = 0x48`:
```python
    DEBUG_SLAVE = 0x49
```

Add immediately after `DEBUG_CONN_PARAMS = 0xA9` (in the Response section):
```python
    DEBUG_SLAVE = 0xAA
```

Add `Command.DEBUG_SLAVE` to `EXPERIMENTAL_COMMANDS` (it is debug-only):

```python
EXPERIMENTAL_COMMANDS = (
    Command.JAM_CONTINUOUS,
    Command.JAM_STOP,
    Command.GATT_SERVE_TABLE,
    Command.DEBUG_SLAVE,
)
```

(Verify the existing tuple membership with `grep -n "EXPERIMENTAL_COMMANDS" python/feralrf/enums.py` and adapt — the tuple may already have other entries from F20.a.1.)

- [ ] **Step 2: Add dataclasses + `Radio.debug_slave()` to `radio.py`**

Find the existing dataclass section in `python/feralrf/radio.py`:

```bash
grep -n "^@dataclass\|^class .*Result\|^class .*Status" python/feralrf/radio.py | head -10
```

Add near the top with other dataclasses (e.g. after `ConnectionResult`):

```python
@dataclass
class SlaveDbgEntry:
    """One slave-event RX snapshot from the firmware ring buffer (F20.a.1.b)."""

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
    """Slave-side diagnostic dump from CMD_DEBUG_SLAVE (F20.a.1.b)."""

    access_addr: int
    crc_init: int
    win_offset: int
    hop_interval: int
    latency: int
    superv_timeout: int
    hop_increment: int
    connect_ind_end_rat: int
    first_anchor_rat: int
    entries: list


@dataclass
class CentralDbgConnParams:
    """Central-side conn-params dump from CMD_DEBUG_CONN_PARAMS (F8A telemetry)."""

    access_addr: int
    crc_init: int
    channel_map: bytes  # 5 bytes
    hop_increment: int
    win_offset: int
    event_counter: int
    conn_time: int
    conn_interval: int
    superv_timeout: int
    use_csa2: bool
    connected: bool
    ll_data: bytes  # raw 22 bytes
```

Add the `debug_slave` and `debug_conn_params` methods near the existing `_send_command`/`_read_response` users (e.g. after `serve_gatt`):

```python
    def debug_slave(self) -> SlaveDbgResult:
        """F20.a.1.b — query slave-side diagnostic dump.

        Returns the snapshot of CONNECT_IND values parsed by the slave plus
        a ring of the most recent per-event RX stats. Used by the smoke V2
        harness to diff slave-parsed values against the central's actuals
        and to spot-check radio behavior. Debug-only API; not in the stable
        command set.
        """
        self._send_command(Command.DEBUG_SLAVE, b"")
        cmd_id, _seq, payload = self._read_response(
            timeout=2.0, expected={Response.DEBUG_SLAVE, Response.ERROR}
        )
        if cmd_id == Response.ERROR:
            raise CommandError("DEBUG_SLAVE failed", payload[0] if payload else 0)
        if len(payload) < 26:
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
        count = payload[25]
        entries = []
        for i in range(count):
            base = 26 + i * 17
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
            entries=entries,
        )

    def debug_conn_params(self) -> CentralDbgConnParams:
        """F8A telemetry — central-side connection parameters dump."""
        self._send_command(Command.DEBUG_CONN_PARAMS, b"")
        cmd_id, _seq, payload = self._read_response(
            timeout=2.0, expected={Response.DEBUG_CONN_PARAMS, Response.ERROR}
        )
        if cmd_id == Response.ERROR:
            raise CommandError("DEBUG_CONN_PARAMS failed", payload[0] if payload else 0)
        if len(payload) < 50:
            raise ProtocolError(f"DEBUG_CONN_PARAMS payload too short: {len(payload)} bytes")
        return CentralDbgConnParams(
            access_addr=int.from_bytes(payload[0:4], "little"),
            crc_init=int.from_bytes(payload[4:8], "little"),
            channel_map=bytes(payload[8:13]),
            hop_increment=payload[13],
            win_offset=int.from_bytes(payload[14:16], "little"),
            event_counter=int.from_bytes(payload[16:18], "little"),
            conn_time=int.from_bytes(payload[18:22], "little"),
            conn_interval=int.from_bytes(payload[22:24], "little"),
            superv_timeout=int.from_bytes(payload[24:26], "little"),
            use_csa2=bool(payload[26]),
            connected=bool(payload[27]),
            ll_data=bytes(payload[28:50]),
        )
```

Verify `CommandError` and `ProtocolError` are imported at the top of `radio.py` (they should already be — F20.a.1 used them).

- [ ] **Step 3: Add unit tests for the parser**

Create `python/tests/test_radio_debug_slave.py`:

```python
"""F20.a.1.b — unit tests for Radio.debug_slave parser (no hardware)."""

from typing import List, Optional, Tuple

import pytest

from feralrf.enums import Command, Response
from feralrf.protocol import build_frame, cobs_decode, parse_frame
from feralrf.radio import Radio, SlaveDbgEntry, SlaveDbgResult


class FakeSerial:
    def __init__(self) -> None:
        self.is_open = True
        self.written: bytearray = bytearray()
        self._read_buf: bytearray = bytearray()
        self.timeout: Optional[float] = None

    def write(self, data: bytes) -> int:
        self.written.extend(data)
        return len(data)

    def flush(self) -> None:
        pass

    def read(self, n: int = 1) -> bytes:
        if not self._read_buf:
            return b""
        out = bytes(self._read_buf[:n])
        del self._read_buf[:n]
        return out

    def reset_input_buffer(self) -> None:
        self._read_buf.clear()

    def reset_output_buffer(self) -> None:
        self.written.clear()

    def close(self) -> None:
        self.is_open = False

    def queue_response(self, cmd_id: int, seq: int, payload: bytes = b"") -> None:
        self._read_buf.extend(build_frame(cmd_id, seq, payload))


def _radio_with_fake_serial() -> Tuple[Radio, FakeSerial]:
    radio = Radio(port="/dev/null")
    fake = FakeSerial()
    radio._serial = fake  # type: ignore[assignment]
    return radio, fake


def _build_payload(snapshot: dict, entries: List[dict]) -> bytes:
    """Build a synthetic RSP_DEBUG_SLAVE payload for testing."""
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


class TestDebugSlaveParser:
    def test_empty_ring(self):
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x12345678,
            "crc_init": 0x00ABCDEF,
            "win_offset": 5,
            "hop_interval": 24,
            "latency": 0,
            "superv_timeout": 100,
            "hop_increment": 7,
            "connect_ind_end_rat": 0x4F00_0000,
            "first_anchor_rat": 0x4F00_5000,
        }
        payload = _build_payload(snap, [])
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert isinstance(result, SlaveDbgResult)
        assert result.access_addr == 0x12345678
        assert result.crc_init == 0x00ABCDEF
        assert result.win_offset == 5
        assert result.hop_interval == 24
        assert result.hop_increment == 7
        assert result.connect_ind_end_rat == 0x4F00_0000
        assert result.first_anchor_rat == 0x4F00_5000
        assert result.entries == []

    def test_two_entries(self):
        radio, fake = _radio_with_fake_serial()
        snap = {
            "access_addr": 0x11223344,
            "crc_init": 0x00112233,
            "win_offset": 6,
            "hop_interval": 24,
            "latency": 0,
            "superv_timeout": 100,
            "hop_increment": 5,
            "connect_ind_end_rat": 1000,
            "first_anchor_rat": 7000,
        }
        entries = [
            {
                "event_counter": 1,
                "chan": 5,
                "anchor_rat": 7000,
                "actual_start_rat": 7000,
                "status": 0x1A03,
                "n_rx_ok": 0,
                "n_rx_nok": 0,
                "n_rx_ignored": 0,
                "pkt_status": 0,
            },
            {
                "event_counter": 2,
                "chan": 10,
                "anchor_rat": 12000,
                "actual_start_rat": 12100,
                "status": 0x1A04,
                "n_rx_ok": 1,
                "n_rx_nok": 0,
                "n_rx_ignored": 0,
                "pkt_status": 0x01,
            },
        ]
        payload = _build_payload(snap, entries)
        fake.queue_response(Response.DEBUG_SLAVE, seq=radio._seq, payload=payload)
        result = radio.debug_slave()
        assert len(result.entries) == 2
        assert result.entries[0].event_counter == 1
        assert result.entries[0].n_rx_ok == 0
        assert result.entries[1].event_counter == 2
        assert result.entries[1].chan == 10
        assert result.entries[1].n_rx_ok == 1
        assert result.entries[1].pkt_status == 0x01
```

- [ ] **Step 4: Run unit tests**

```bash
cd python
PYTHONPATH=. pytest tests/test_radio_debug_slave.py -v 2>&1 | tail -10
PYTHONPATH=. pytest -q 2>&1 | tail -3
```

Expected: 2 new tests pass. Full suite ≥ 588 (was 586 + 2). No regressions.

- [ ] **Step 5: Pre-commit + commit Bundle 3**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/feralrf/enums.py python/feralrf/radio.py python/tests/test_radio_debug_slave.py
git add python/feralrf/enums.py python/feralrf/radio.py python/tests/test_radio_debug_slave.py
git commit -m "$(cat <<'EOF'
feat(f20.a.1.b): Python debug_slave + debug_conn_params APIs

Radio.debug_slave() parses RSP_DEBUG_SLAVE (slave-side CONNECT_IND
snapshot + per-event RX ring). Radio.debug_conn_params() parses the
existing F8A central-side dump for symmetric host comparison.
Adds 3 dataclasses (SlaveDbgEntry, SlaveDbgResult, CentralDbgConnParams)
and 2 unit tests covering the wire-format parser. Command.DEBUG_SLAVE
joins EXPERIMENTAL_COMMANDS — debug-only, no stable-API contract.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Smoke V2 harness — diff + ring summary

**Files:**
- Create: `python/examples/smoke_f20a1_b_diag.py`

- [ ] **Step 1: Create the harness**

```python
#!/usr/bin/env python3
"""F20.a.1.b — Smoke V2 NOSYNC diagnostic.

Runs the F20.a.1 smoke (peripheral advertise + central connect), waits for
disconnect, then queries both boards for diagnostic dumps and diffs them.

Pass criteria:
  1. All compared fields match (accessAddr, crcInit, hopIncrement,
     winOffset, hopInterval, supervTimeout).
  2. At least one slave ring entry has nRxOk > 0 (slave actually received
     a packet from master).
  3. GATT discovery completes (services >= 2, name == FERAL_GATT,
     test == HELLO_FERAL).

Run:
    python smoke_f20a1_b_diag.py --peripheral-port /dev/ttyACM0 \\
        --central-port /dev/ttyACM3
"""
import argparse
import re
import sys
import time
from threading import Thread

import serial

from feralrf import RxStreamError
from feralrf.radio import Radio


def reset_cc1352(port: str) -> None:
    m = re.search(r"(\d+)$", port)
    if not m:
        return
    shell = port[: m.start(1)] + str(int(m.group(1)) + 2)
    try:
        s = serial.Serial(shell, 115200, timeout=1.0, write_timeout=1.0)
        s.write(b"boot\r\n")
        time.sleep(0.5)
        s.write(b"exit\r\n")
        time.sleep(0.3)
        s.close()
    except Exception:
        pass
    time.sleep(3.5)


def run_peripheral(port: str, baud: int, target_addr: str, count: int) -> None:
    radio = Radio(port=port, baudrate=baud)
    try:
        radio.init()
        radio.serve_gatt()
        radio.advertise_ind(
            payload=b"\x02\x01\x06",
            scan_resp_data=b"FERAL_GATT_SR",
            target_addr=target_addr,
            count=count,
            interval_us=10000,
        )
    finally:
        radio.disconnect()


def run_central_attempt(port: str, baud: int, target_addr: str) -> dict:
    """Return dict with services_count, chars_count, name_val, test_val,
    dc_reason (or None)."""
    addr_le = bytes(int(p, 16) for p in reversed(target_addr.split(":")))
    radio = Radio(port=port, baudrate=baud)
    out = {"services_count": 0, "chars_count": 0, "name_val": b"", "test_val": b"",
           "dc_reason": None, "connected": False}
    try:
        radio.init()
        radio.reset_device()
        radio.init()
        result = radio.ble_connect(addr_le, addr_type=1, timeout=10.0)
        if not result.is_ok:
            return out
        out["connected"] = True
        try:
            services = radio.gatt_discover(timeout=10.0)
            out["services_count"] = len(services.services)
            out["chars_count"] = len(services.characteristics)
            try:
                out["name_val"] = radio.gatt_read(handle=3, timeout=5.0)
            except Exception:
                pass
            try:
                out["test_val"] = radio.gatt_read(handle=6, timeout=5.0)
            except Exception:
                pass
        except Exception:
            pass
        try:
            radio.ble_disconnect(timeout=2.0)
        except Exception:
            pass
        return out
    finally:
        radio.disconnect()


def query_diagnostics(per_port: str, cen_port: str, baud: int) -> tuple:
    """After disconnect, query both boards for their diagnostic dumps."""
    per = Radio(port=per_port, baudrate=baud)
    cen = Radio(port=cen_port, baudrate=baud)
    try:
        slave_dump = per.debug_slave()
        central_dump = cen.debug_conn_params()
        return slave_dump, central_dump
    finally:
        per.disconnect()
        cen.disconnect()


def diff_table(slave, central) -> tuple:
    """Compare slave-parsed vs central-actual values. Returns
    (all_match: bool, lines: list[str])."""
    fields = [
        ("accessAddr", f"0x{slave.access_addr:08X}", f"0x{central.access_addr:08X}",
         slave.access_addr == central.access_addr),
        ("crcInit", f"0x{slave.crc_init:06X}", f"0x{central.crc_init:06X}",
         slave.crc_init == central.crc_init),
        ("hopIncrement", str(slave.hop_increment), str(central.hop_increment),
         slave.hop_increment == central.hop_increment),
        ("winOffset", str(slave.win_offset), str(central.win_offset),
         slave.win_offset == central.win_offset),
        ("hopInterval", str(slave.hop_interval), str(central.conn_interval),
         slave.hop_interval == central.conn_interval),
        ("supervTimeout", str(slave.superv_timeout), str(central.superv_timeout),
         slave.superv_timeout == central.superv_timeout),
    ]
    lines = [f"{'Field':<16} {'Slave':<16} {'Central':<16} Match"]
    lines.append("-" * 60)
    all_match = True
    for name, s, c, ok in fields:
        mark = "✓" if ok else "✗"
        lines.append(f"{name:<16} {s:<16} {c:<16} {mark}")
        if not ok:
            all_match = False
    return all_match, lines


def main() -> int:
    parser = argparse.ArgumentParser(description="F20.a.1.b smoke V2 NOSYNC diagnostic")
    parser.add_argument("--peripheral-port", required=True)
    parser.add_argument("--central-port", required=True)
    parser.add_argument("--baudrate", type=int, default=921600)
    parser.add_argument("--target-mac", default="DE:AD:BE:EF:CA:FE")
    parser.add_argument("--peripheral-count", type=int, default=5000,
                        help="ADV iterations on peripheral (default ~50s buffer).")
    args = parser.parse_args()

    print("F20.a.1.b smoke V2 — NOSYNC diagnostic")
    print(f"Peripheral={args.peripheral_port} Central={args.central_port}")
    print(f"Target MAC={args.target_mac}")
    print("=" * 60)

    reset_cc1352(args.peripheral_port)
    reset_cc1352(args.central_port)

    per_thread = Thread(
        target=run_peripheral,
        args=(args.peripheral_port, args.baudrate, args.target_mac, args.peripheral_count),
        daemon=True,
    )
    per_thread.start()
    time.sleep(2.0)

    cen_result = run_central_attempt(args.central_port, args.baudrate, args.target_mac)
    per_thread.join(timeout=10.0)

    print("\n--- Central run ---")
    print(f"connected: {cen_result['connected']}")
    print(f"services discovered: {cen_result['services_count']}")
    print(f"chars discovered: {cen_result['chars_count']}")
    print(f"device name: {cen_result['name_val']!r}")
    print(f"test value: {cen_result['test_val']!r}")

    print("\n--- Querying diagnostic dumps ---")
    try:
        slave, central = query_diagnostics(
            args.peripheral_port, args.central_port, args.baudrate
        )
    except Exception as e:
        print(f"diagnostic query FAILED: {e!r}")
        return 1

    print("\n--- Field diff (slave parsed vs central actual) ---")
    all_match, lines = diff_table(slave, central)
    for line in lines:
        print(line)

    print("\n--- Slave RX ring (oldest first) ---")
    if not slave.entries:
        print("  (empty — startSlave never ran or pollSlave never completed an event)")
    else:
        print(
            f"{'evt':>4} {'chan':>4} {'anchor':>10} {'actual':>10} "
            f"{'Δus':>6} {'status':>6} {'nRxOk':>5} {'nRxNok':>6} "
            f"{'nRxIgn':>7} {'pktSt':>5}"
        )
        prev_anchor = None
        for e in slave.entries:
            delta_us = (e.actual_start_rat - e.anchor_rat) // 4
            interval_us = "" if prev_anchor is None else str((e.anchor_rat - prev_anchor) // 4)
            print(
                f"{e.event_counter:>4} {e.chan:>4} 0x{e.anchor_rat:08x} "
                f"0x{e.actual_start_rat:08x} {delta_us:>6} 0x{e.status:04x} "
                f"{e.n_rx_ok:>5} {e.n_rx_nok:>6} {e.n_rx_ignored:>7} 0x{e.pkt_status:02x}"
            )
            prev_anchor = e.anchor_rat

    any_rx = any(e.n_rx_ok > 0 for e in slave.entries)
    gatt_pass = (
        cen_result["services_count"] >= 2
        and cen_result["name_val"] == b"FERAL_GATT"
        and cen_result["test_val"] == b"HELLO_FERAL"
    )

    print("\n" + "=" * 60)
    print(f"[{'PASS' if all_match else 'FAIL'}] all parsed fields match central actuals")
    print(f"[{'PASS' if any_rx else 'FAIL'}] slave received >=1 packet from master (any nRxOk>0)")
    print(f"[{'PASS' if gatt_pass else 'FAIL'}] GATT path: services>=2, name+test correct")

    overall = all_match and any_rx and gatt_pass
    print(f"\n[{'PASS' if overall else 'FAIL'}] Smoke V2 overall")
    return 0 if overall else 1


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Verify parse + --help**

```bash
cd python
PYTHONPATH=. python -c "import ast; ast.parse(open('examples/smoke_f20a1_b_diag.py').read())" && echo "parse OK"
PYTHONPATH=. python examples/smoke_f20a1_b_diag.py --help 2>&1 | head -5
```

Expected: `parse OK` + usage text.

- [ ] **Step 3: Pre-commit + commit Bundle 4**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/examples/smoke_f20a1_b_diag.py
git add python/examples/smoke_f20a1_b_diag.py
git commit -m "$(cat <<'EOF'
test(f20.a.1.b): smoke V2 NOSYNC diagnostic harness

Runs F20.a.1 smoke + queries both boards for telemetry on disconnect.
Diffs slave-parsed vs central-actual fields, prints ring summary
(per-event chan, anchor delta, status, RX counts), and asserts pass
criteria (field match + nRxOk>0 + GATT path).

Hardware run pending HUMAN CHECKPOINT.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: HUMAN CHECKPOINT — flash + first iteration run

**Files:** None (hardware operation)

- [ ] **Step 1: PAUSE — hand control to user for board flash**

The agent does NOT run flash or smoke autonomously. The user runs:

```bash
ls /dev/ttyACM*
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip devices
```

Expected: 2 CatSniffer devices listed. Re-detect every session — USB enumeration shifts.

```bash
python -m catnip flash <peripheral-bridge-port> /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
python -m catnip flash -d 2 <central-bridge-port> /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

Per memory `feedback_flash_retry`: retry 2× before manual reset.

- [ ] **Step 2: User runs smoke V2**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
PYTHONPATH=. python examples/smoke_f20a1_b_diag.py \
    --peripheral-port <PER_PORT> --central-port <CEN_PORT> 2>&1 | tee /tmp/smoke_v2_run1.txt
```

Expected outcomes (one of):
- **All pass:** Smoke V2 prints `[PASS]` overall. Skip to Task 7.
- **Field mismatch:** diff table shows `✗` on one or more fields. Proceed to Task 6 with the mismatched field as the iteration focus.
- **Fields match, no nRxOk:** all field rows are `✓` but `slave received >=1 packet from master` is FAIL. Proceed to Task 6 with radio behavior as the focus.
- **Empty ring:** `slave RX ring (empty)` printed. Means `BleConnMgr_pollSlave` never completed an iteration — likely a hang in `RadioIF_bleSlave` itself. Proceed to Task 6 with radio-call hang as the focus.

The agent reads `/tmp/smoke_v2_run1.txt` to interpret signals before starting Task 6.

---

## Task 6: Iteration cycle (template — repeat per fix)

This task is a template the agent applies once per identified fix. Each pass produces ONE commit that addresses ONE root cause signal from the most recent smoke run.

**Files:** Whichever single firmware module owns the bug (most likely one of):
- `firmware/cc1352/src/radio_if.c` (parser bug, slave RX queue bug)
- `firmware/cc1352/src/ble_conn_mgr.c` (anchor/timing bug, channel calc bug)
- `firmware/cc1352/src/smartrf_ble5_0.c` (rxConfig / pParams bug)

- [ ] **Step 1: Inspect the most recent smoke V2 output**

Read `/tmp/smoke_v2_run<N>.txt`. Identify the SINGLE highest-signal anomaly:

- Mismatched field → fix the parser. Look at `RadioIF_extractConnectIndParams` — likely a wrong byte offset or endianness.
- All fields match, `actual_start - anchor` is consistently > 1ms → timing fix in `BleConnMgr_pollSlave` (catch-up, anchor formula).
- All fields match, `chan` progression doesn't match what `(event_counter+1) * hop_increment % 37` would compute given the central's `hop_increment` → CSA#1 formula bug.
- All fields match, `status` is a specific TI error code → look it up in `rf_ble_cmd.h` (e.g. `BLE_DONE_NOSYNC=0x1A03`, `BLE_DONE_RXTIMEOUT=0x1A04`).
- All fields match, ring is healthy except `nRxOk` always 0 → slave-side rxConfig issue (e.g. `bIncludeLenByte` mismatch with master expectations) or `accessAddress`/`crcInit` not reaching the radio (verify `Ble5_0_cmdBle5Slave.pParams->accessAddress` == parsed value).
- Empty ring + smoke hangs → `RadioIF_bleSlave` hang. Add a `RF_runCmd` timeout (`pParams->endTrigger.triggerType = TRIG_ABSTIME` is already set; verify `endTime` is reasonable).

- [ ] **Step 2: Apply the fix**

ONE focused change, with a brief inline comment explaining the signal that motivated it.

- [ ] **Step 3: Build**

```bash
cmake --build firmware/cc1352/build -j2 2>&1 | tail -5
```

Expected: clean.

- [ ] **Step 4: Pre-commit + commit**

```bash
pre-commit run --files <affected files>
git add <affected files>
git commit -m "fix(f20.a.1.b): <one-line summary of the signal that motivated this fix>

<Detail: what telemetry showed, what was changed, why this maps to the
signal. Reference the smoke run output if helpful (run<N>).>

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 5: HUMAN CHECKPOINT — re-flash + re-run smoke**

Same as Task 5. User flashes both boards, runs smoke, captures output to `/tmp/smoke_v2_run<N+1>.txt`.

If smoke V2 PASSES → proceed to Task 7.
If still failing → loop back to Task 6 Step 1 with the new output.

**Iteration budget:** 5 passes maximum before re-evaluating scope. If 5 fixes don't converge, escalate to user — likely needs a deeper architectural revisit (e.g. running `BleConnMgr_pollSlave` from `RfTask` instead of command-processor task; F8c/F8d-style cooperative scheduling).

---

## Task 7: Closing — drop partial tag, FF merge, memory

**Files:**
- Memory: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f20a1_done.md` (new)
- Memory: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f20a1_partial.md` (delete or keep with "SUPERSEDED" prefix in description)
- Memory: `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md` (update index)

- [ ] **Step 1: Verify smoke V2 fully passes one final time**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
PYTHONPATH=. python examples/smoke_f20a1_b_diag.py \
    --peripheral-port <PER_PORT> --central-port <CEN_PORT>
```

Expected: `[PASS] Smoke V2 overall`.

- [ ] **Step 2: Tag the new HEAD as `v2.0-f20.a.1`**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git tag -a v2.0-f20.a.1 -m "F20.a.1 BLE Peripheral Read-only — closed.

Smoke V1 + V2 PASS on hardware. Slave receives master TX, GATT
discovery completes (services >= 2, name == FERAL_GATT, test ==
HELLO_FERAL), connection sustains past supervTimeout window. Closes
the F20.a.1 line; v2.0-f20.a.1-partial superseded.

F20.a.1.b iteration cycle resolved the NOSYNC blocker via slave
telemetry (CMD_DEBUG_SLAVE 0x49) + diff-driven fixes."
git tag --list "v2.0-f20*"
```

Expected: both `v2.0-f20.a.1-partial` and `v2.0-f20.a.1` tags present.

- [ ] **Step 3: FF merge to main**

```bash
git checkout main
git pull --ff-only origin main 2>/dev/null || true
git merge --ff-only feature/f20a1-peripheral-read
git log --oneline -5
```

Expected: main HEAD now equals branch HEAD. If FF refuses (main has diverged), STOP and escalate to user.

- [ ] **Step 4: Write `project_f20a1_done.md` and update `MEMORY.md`**

Create `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f20a1_done.md`:

```markdown
---
name: project_f20a1_done
description: F20.a.1 closed 2026-05-04. Tag v2.0-f20.a.1 (replaces -partial). Smoke V2 PASS — slave receives master TX, GATT discovery + read works. F20.a.1.b debug telemetry framework lives in firmware as CMD_DEBUG_SLAVE 0x49.
type: project
---

F20.a.1 BLE Peripheral Read-only CLOSED 2026-05-04.

[Replace this with concrete details from the actual closing iteration:
which fix(es) resolved NOSYNC, which iteration count, which signals
the diff/ring revealed, etc. The agent fills this in based on the
actual iteration record from Task 6.]

Tag v2.0-f20.a.1 on `feature/f20a1-peripheral-read` (FF'd into main).
v2.0-f20.a.1-partial retained for history.

Telemetry: CMD_DEBUG_SLAVE (0x49) → RSP_DEBUG_SLAVE (0xAA) remains in
firmware as a debugging primitive (EXPERIMENTAL_COMMANDS). May be
removed or gated behind a build flag in a future cleanup pass.

F20.a.2 (Write Req + HVN/Indicate) now unblocked. F20.b (dynamic GATT
table) likewise.
```

Update `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/MEMORY.md`:

Replace the line:
```
- [project_f20a1_partial.md](project_f20a1_partial.md) — 2026-05-04 F20.a.1 PARTIAL...
```

With:
```
- [project_f20a1_done.md](project_f20a1_done.md) — 2026-05-04 F20.a.1 CLOSED. Tag v2.0-f20.a.1, FF'd to main. Smoke V2 PASS via F20.a.1.b telemetry-driven NOSYNC fix.
```

Optionally keep the partial entry as historical with a "SUPERSEDED" prefix in its `description:` frontmatter.

- [ ] **Step 5: Push (only when user explicitly authorizes)**

```bash
git push origin main
git push origin v2.0-f20.a.1
```

Per project memory: do NOT push without explicit user approval. Confirm with user before this step.

---

## Task 8: Optional cleanup — gate or remove debug telemetry

**Files:**
- Decision deferred (separate cleanup PR after this branch lands)

The `CMD_DEBUG_SLAVE` command and the slave debug ring add code that is only useful during NOSYNC investigation. Once F20.a.1 is closed, options:

- Keep as-is (debug command always available, ~250 B firmware overhead).
- Gate behind `#ifdef FERALRF_F20_DEBUG_SLAVE` build flag.
- Remove entirely (the smoke V2 harness becomes useless without it).

Recommendation: **keep as-is** for the duration of F20.a.2 / F20.b development. Revisit at F20 phase close. No work in this plan.

---

## Self-Review

Spec coverage: ✓ all 7 spec sections (problem, goal, architecture, wire format, components, smoke V2 pass criteria, iteration cycle, closing) have a corresponding task. Risks block (response-size constraint, ring-race, snapshot-safety, multi-iteration cost) addressed inline.

Placeholder scan: ✓ no TBD/TODO; all code blocks complete; iteration template (Task 6) intentionally parameterized but with concrete step structure.

Type consistency: ✓ wire offsets in Task 1/2 firmware match Task 3 Python parser; `BleConnMgr_DbgSlaveEntry` field names consistent across tasks; `SlaveDbgEntry` Python field names map directly to wire bytes via `_build_payload` test helper.
