# F8A Session 6 — GATT RX-queue saturation fix

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Surface non-empty ATT responses to the host so `gatt_discover()` can complete against CH573, then tag `v2.0-f8a`.

**Architecture:** Master CMD_BLE5_MASTER reuses Sniffle's RX data queue (3 entries) with `bAutoFlushEmpty=0`. Sniffle is a passive listener that needs every packet including empties; our master use case does **not** — empties are already reflected in `pOutput.pktStatus.bLastEmpty`. The 3-entry queue saturates within 3 connection events when slave keeps ACKing with empty PDUs, so the rare non-empty ATT response is dropped on arrival. Smallest-possible test of the hypothesis: flip `bAutoFlushEmpty` to 1. If GATT round-trips after the flip, the saturation theory is confirmed; if it doesn't, broader RX/TX queue introspection is added (Phase 3 fallback).

**Tech Stack:** CC1352P7 firmware (TI-RTOS 7, SDK 8.30, GCC), CMSIS-DAP via OpenOCD for flashing, FeralRF Python API for hardware tests, Sniffle on board #2 as on-air ground truth.

---

## Pre-flight (already passed)

- ✅ Branch `feature/f8a-ble-central-sniffle` HEAD `0372cc5`, tree clean
- ✅ Board #1 (FeralRF CC1352) = `/dev/ttyACM8`, Board #2 (Sniffle CC1352) = `/dev/ttyACM5`
- ✅ Anchor regression negative: `connected=True events=85 status=0x1400 tx_done=85 total_rx=0`. The `total_rx=0` reproduces the exact GATT bug to fix.

## Hypothesis under test

**H_QUEUE_SAT:** With `bAutoFlushEmpty=0` and `RF_QUEUE_NUM_DATA_ENTRIES=3`, slave's empty PDU ACKs (1/event) saturate the master RX queue within 3 events. Subsequent slave PDUs — including non-empty ATT responses — are dropped by the RF core because no entry is `DATA_ENTRY_PENDING`. They never reach `RadioIF_processBlePackets`, so `total_rx` stays at 0.

**Falsification criterion:** Flipping `bAutoFlushEmpty` to 1 and re-running `gatt_discover()` against CH573 — if `RSP_GATT_SERVICE` arrives, hypothesis confirmed; if not, hypothesis is wrong (or insufficient) and Phase 3 instrumentation runs.

---

## Task 1 — Smallest test: flip `bAutoFlushEmpty` to 1

**Files:**
- Modify: `firmware/cc1352/src/radio_if.c:2324`

- [ ] **Step 1: Capture baseline `total_rx` post-`gatt_discover`**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
timeout 12 .venv/bin/python -c "
from feralrf import Radio
import time
r = Radio('/dev/ttyACM8'); r.connect(); time.sleep(0.3); r.init()
addr = bytes(reversed(bytes.fromhex('DC32628DE109')))
r.ble_connect(addr, addr_type=0); time.sleep(1.0)
print('--- pre-gatt:', r.conn_status())
try:
    svcs = r.gatt_discover(timeout=4.0)
    print('--- gatt_discover OK, services:', len(svcs))
except Exception as e:
    print('--- gatt_discover FAIL:', e)
print('--- post-gatt:', r.conn_status())
r.ble_disconnect(); r.disconnect()
" 2>&1 | tee /tmp/f8a-s6-baseline.txt
```

Expected (baseline): `gatt_discover FAIL: timeout`, `total_rx=0`.

- [ ] **Step 2: Apply the bit flip**

Edit `firmware/cc1352/src/radio_if.c:2324`:

```c
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushEmpty = 1;
```

(was `0`). Add a one-line comment immediately above explaining the why — this is the kind of comment the project policy allows because the WHY is non-obvious:

```c
    /* Master mode: empties already surface via pOutput.pktStatus.bLastEmpty.
     * Flushing them keeps the 3-entry RX queue from saturating before slave
     * sends a non-empty ATT response. */
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushEmpty = 1;
```

- [ ] **Step 3: Build CC1352 firmware**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
make -j$(nproc) 2>&1 | tail -20
```

Expected: `feralrf_cc1352.hex` updated, no warnings/errors.

- [ ] **Step 4: Flash board #1**

Use catnip (per `feedback_workflow` — always retry 2× before asking user):

```bash
python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py flash \
    --port /dev/ttyACM8 \
    --file /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

If it fails, retry once more. Only ask the user to physically reset if 2 retries fail.

- [ ] **Step 5: Re-run the GATT test, capture `total_rx`**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
timeout 12 .venv/bin/python -c "
from feralrf import Radio
import time
r = Radio('/dev/ttyACM8'); r.connect(); time.sleep(0.3); r.init()
addr = bytes(reversed(bytes.fromhex('DC32628DE109')))
r.ble_connect(addr, addr_type=0); time.sleep(1.0)
print('--- pre-gatt:', r.conn_status())
try:
    svcs = r.gatt_discover(timeout=4.0)
    print('--- gatt_discover OK, services:', len(svcs))
    for s in svcs[:5]:
        print('   ', s)
except Exception as e:
    print('--- gatt_discover FAIL:', e)
print('--- post-gatt:', r.conn_status())
r.ble_disconnect(); r.disconnect()
" 2>&1 | tee /tmp/f8a-s6-postfix.txt
```

- [ ] **Step 6: Decision gate**

Read `/tmp/f8a-s6-postfix.txt`:

| Outcome | `total_rx` | `gatt_discover` | Next |
|---------|-----------|-----------------|------|
| ✅ Hypothesis confirmed | > 0 | OK, services > 0 | Skip Task 2, go to Task 4 |
| ⚠️ Partial | > 0 | timeout but `total_rx > 0` | Go to Task 3 (different bug above queue) |
| ❌ Hypothesis falsified | 0 | timeout | Go to Task 2 (instrument) |

- [ ] **Step 7: Commit if (and only if) ✅ outcome**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --all-files   # never skip
git add firmware/cc1352/src/radio_if.c
git commit -m "$(cat <<'EOF'
fix(f8a): flush empty PDUs from master RX queue

The 3-entry RF data queue saturated within 3 connection events when
slave kept sending empty PDU ACKs (bAutoFlushEmpty=0 was inherited
from Sniffle's passive-sniffer use case). Non-empty ATT responses
were dropped on arrival because no queue entry was DATA_ENTRY_PENDING.

Flipping bAutoFlushEmpty=1 unblocks the GATT round-trip;
pOutput.pktStatus.bLastEmpty still surfaces empty ACKs to host
telemetry, so we lose nothing.

Closes F8A: gatt_discover now returns services from CH573.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"
```

If the ⚠️ or ❌ outcome triggered, **don't commit yet** — Tasks 2/3 will guide further work and combine into a single commit when green.

---

## Task 2 — Fallback: instrument RX queue (only if Task 1 ❌)

This task only runs if Task 1 Step 6 was ❌ (`total_rx=0` after the flip). The hypothesis was wrong; we need to see what's actually in the RX queue to form a new one.

**Files:**
- Modify: `firmware/cc1352/src/radio_if.c` (add `RadioIF_dumpBleRxQueue` + `RadioIF_dumpBleTxQueue` accessors)
- Modify: `firmware/cc1352/include/radio_if.h` (export the accessors)
- Modify: `firmware/cc1352/src/command_processor.c` (add `CMD_DEBUG_RX_QUEUE` 0x49 / `RSP_DEBUG_RX_QUEUE` 0xAA, `CMD_DEBUG_TX_QUEUE` 0x4A / `RSP_DEBUG_TX_QUEUE` 0xAB)
- Modify: `python/feralrf/protocol.py` and `python/feralrf/radio.py` (add `r.debug_rx_queue()` / `r.debug_tx_queue()` helpers)

- [ ] **Step 1: Add RX queue dump accessor**

In `firmware/cc1352/src/radio_if.c`, add after `RadioIF_bleDrainRxQueue` (around line 2391):

```c
/* Debug: dump up to 3 RX data queue entries (status, length, first 32 bytes
 * of payload) WITHOUT consuming them. Caller buffer must be ≥ (3 * 36) bytes;
 * returns count actually written. Walks via the same ring s_rf_data_queue
 * uses, starting at s_rf_read_entry. */
uint8_t RadioIF_dumpBleRxQueue(uint8_t *out, uint8_t maxBytes) {
    if (out == NULL || maxBytes < 3u * 36u) {
        return 0;
    }
    rfc_dataEntryGeneral_t *e = s_rf_read_entry;
    uint8_t count = 0;
    for (uint8_t i = 0; i < RF_QUEUE_NUM_DATA_ENTRIES; i++) {
        uint8_t *p = out + ((uint16_t)i * 36u);
        if (e == NULL) {
            memset(p, 0, 36);
            continue;
        }
        p[0] = e->status;
        uint8_t *raw = (uint8_t *)&e->data;
        uint16_t entry_len = (uint16_t)raw[0] | ((uint16_t)raw[1] << 8);
        p[1] = (uint8_t)(entry_len & 0xFFu);
        p[2] = (uint8_t)((entry_len >> 8) & 0xFFu);
        uint8_t copy = (entry_len > 32u) ? 32u : (uint8_t)entry_len;
        memcpy(&p[3], raw + 2, copy);
        if (copy < 32u) {
            memset(&p[3 + copy], 0, 32u - copy);
        }
        count++;
        e = (rfc_dataEntryGeneral_t *)e->pNextEntry;
    }
    return count;
}
```

- [ ] **Step 2: Add TX queue dump accessor**

In `firmware/cc1352/src/tx_queue.c`, add after `TXQueue_flush`:

```c
/* Debug: dump up to TX_QUEUE_SIZE entries (status, length, first 17 bytes:
 * 1 LLID + 16 payload). Caller buffer must be ≥ (TX_QUEUE_SIZE * 20) bytes.
 * Returns count of slots dumped (always TX_QUEUE_SIZE for fixed layout). */
uint8_t TXQueue_dump(uint8_t *out, uint8_t maxBytes) {
    if (out == NULL || maxBytes < TX_QUEUE_SIZE * 20u) {
        return 0;
    }
    for (uint32_t i = 0; i < TX_QUEUE_SIZE; i++) {
        uint8_t *p = out + (i * 20u);
        rfc_dataEntryPointer_t *e = &s_queue_entries[i];
        p[0] = e->status;
        p[1] = e->length;
        if (e->length > 0u && e->pData != NULL) {
            uint8_t copy = (e->length > 18u) ? 18u : e->length;
            memcpy(&p[2], e->pData, copy);
            if (copy < 18u) {
                memset(&p[2 + copy], 0, 18u - copy);
            }
        } else {
            memset(&p[2], 0, 18);
        }
    }
    return TX_QUEUE_SIZE;
}
```

Add prototype to `firmware/cc1352/include/tx_queue.h`:

```c
uint8_t TXQueue_dump(uint8_t *out, uint8_t maxBytes);
```

- [ ] **Step 3: Wire commands**

In `firmware/cc1352/src/command_processor.c`, after the existing `CMD_DEBUG_CONN_PARAMS` block (line 669), add (and add the two `#define` lines near line 52 / 72):

```c
#define CMD_DEBUG_RX_QUEUE 0x49u
#define CMD_DEBUG_TX_QUEUE 0x4Au
#define RSP_DEBUG_RX_QUEUE 0xAAu
#define RSP_DEBUG_TX_QUEUE 0xABu
```

Cases:

```c
    case CMD_DEBUG_RX_QUEUE: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        /* Layout: count(u8) + count*36, where each entry is:
         *   status(u8) entry_len(u16 LE) raw_payload[0..32] */
        uint8_t rsp[1u + 3u * 36u];
        uint8_t n = RadioIF_dumpBleRxQueue(&rsp[1], 3u * 36u);
        rsp[0] = n;
        send_response(RSP_DEBUG_RX_QUEUE, seq, rsp, (uint16_t)(1u + (uint16_t)n * 36u));
        return;
    }
    case CMD_DEBUG_TX_QUEUE: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        /* Layout: count(u8) + count*20, where each entry is:
         *   status(u8) length(u8) payload[0..18]  (payload[0] = LLID, rest = data) */
        uint8_t rsp[1u + 8u * 20u];
        uint8_t n = TXQueue_dump(&rsp[1], 8u * 20u);
        rsp[0] = n;
        send_response(RSP_DEBUG_TX_QUEUE, seq, rsp, (uint16_t)(1u + (uint16_t)n * 20u));
        return;
    }
```

Add `RadioIF_dumpBleRxQueue` prototype to `firmware/cc1352/include/radio_if.h` near the other `RadioIF_ble*` exports.

- [ ] **Step 4: Add Python helpers**

In `python/feralrf/protocol.py`, add `CMD_DEBUG_RX_QUEUE = 0x49`, `CMD_DEBUG_TX_QUEUE = 0x4A`, `RSP_DEBUG_RX_QUEUE = 0xAA`, `RSP_DEBUG_TX_QUEUE = 0xAB` next to the existing debug command IDs.

In `python/feralrf/radio.py`, add two methods on `Radio` next to `debug_timing()` / `debug_conn_params()`:

```python
    def debug_rx_queue(self) -> List[Dict]:
        """Snapshot up to 3 master RX data queue entries (raw, no draining)."""
        rsp = self._cmd_response(CMD_DEBUG_RX_QUEUE, b"", expect=RSP_DEBUG_RX_QUEUE)
        n = rsp[0]
        out = []
        for i in range(n):
            base = 1 + i * 36
            out.append({
                "status": rsp[base],
                "entry_len": rsp[base + 1] | (rsp[base + 2] << 8),
                "payload": bytes(rsp[base + 3 : base + 35]),
            })
        return out

    def debug_tx_queue(self) -> List[Dict]:
        """Snapshot all 8 TXQueue slots (status, length, LLID + first 16 bytes)."""
        rsp = self._cmd_response(CMD_DEBUG_TX_QUEUE, b"", expect=RSP_DEBUG_TX_QUEUE)
        n = rsp[0]
        out = []
        for i in range(n):
            base = 1 + i * 20
            length = rsp[base + 1]
            llid = rsp[base + 2] & 0x3 if length > 0 else None
            out.append({
                "status": rsp[base],
                "length": length,
                "llid": llid,
                "payload": bytes(rsp[base + 3 : base + 20]) if length > 0 else b"",
            })
        return out
```

(Match whatever the existing protocol helper is named in this codebase — the prototype should mirror `debug_conn_params`. If `_cmd_response` isn't the right helper, use `self._send_command` + `self._read_response` exactly like `debug_timing` does.)

- [ ] **Step 5: Build, flash**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -10
python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py flash \
    --port /dev/ttyACM8 \
    --file /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
```

- [ ] **Step 6: Capture paired diagnostic**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
timeout 15 .venv/bin/python -c "
from feralrf import Radio
import time, json
r = Radio('/dev/ttyACM8'); r.connect(); time.sleep(0.3); r.init()
addr = bytes(reversed(bytes.fromhex('DC32628DE109')))
r.ble_connect(addr, addr_type=0); time.sleep(0.5)
print('--- TX queue pre-discover:', r.debug_tx_queue())
print('--- RX queue pre-discover:', r.debug_rx_queue())
try:
    svcs = r.gatt_discover(timeout=4.0)
except Exception as e:
    print('--- gatt_discover FAIL:', e)
print('--- TX queue post-discover:', r.debug_tx_queue())
print('--- RX queue post-discover:', r.debug_rx_queue())
print('--- conn_status:', r.conn_status())
print('--- timing tail:', r.debug_timing()[-3:])
r.ble_disconnect(); r.disconnect()
" 2>&1 | tee /tmp/f8a-s6-instrumented.txt
```

- [ ] **Step 7: Decision gate based on instrumented evidence**

Inspect `/tmp/f8a-s6-instrumented.txt`:

| RX queue contains | TX queue contains | Diagnosis |
|-------------------|-------------------|-----------|
| 3× empty PDUs (`length=0`, payload `02 00`) and an ATT response in slot 0 with `entry_len > 5` | ATT request bytes (LLID 0x02, length≥7) | Hypothesis already confirmed but `processBlePackets` length parser dropped it. Fix `radio_if.c:1271` length extraction for data PDUs. |
| 3× empty PDUs only, no ATT response anywhere | ATT request bytes present | Slave never sends non-empty response. Capture passive Sniffle pcap (Step 8) to confirm wire-side. |
| Mixed entries with non-`DATA_ENTRY_FINISHED` status | ATT request bytes present | Queue is not draining at all — bug is somewhere in the BleConnMgr → RadioIF_bleDrainRxQueue path. |
| TX queue shows length=0 for all slots | — | ATT request was never queued by `AttClient_poll`. Bug is in att_client.c. |

Document the diagnosis in `docs/investigations/2026-04-24-f8a-session-1/session-6-queue-evidence.md` with a paste of the relevant queue dumps and the chosen root cause.

- [ ] **Step 8: (Conditional) passive Sniffle pcap if RX queue shows no ATT response**

Only run if Step 7 needs wire confirmation:

```bash
# Terminal A — start Sniffle on board #2 in passive follow mode:
cd /home/sabas/Documents/electroniccats/Sniffle/python_cli
./sniff_receiver.py -s /dev/ttyACM5 -o /tmp/f8a-s6-conn.pcap

# Terminal B — run the FeralRF GATT test (same as Step 6).

# After ~10 s, Ctrl-C Sniffle. Inspect with:
wireshark /tmp/f8a-s6-conn.pcap   # filter: btatt or btle.data_header.llid != 1
```

Look for: master-side TX of ATT_READ_BY_GROUP_TYPE_REQ on a data channel, then slave's response (or lack thereof).

---

## Task 3 — Targeted fix based on Task 2 diagnosis

Only runs if Task 1 ❌. The exact fix depends on what Task 2 Step 7 surfaced:

- [ ] **Step 1: Implement the fix indicated by the diagnosis table**

If `processBlePackets` length parser dropped data-channel PDUs:
- Edit `radio_if.c:1271` and `:1277` — change the `& 0x3Fu` mask to `& 0xFFu` for data PDUs (data channel uses 8-bit length on BLE 4.2+; the 6-bit mask is leftover from advertising). Inspect `bIncludeLenByte=1` interaction with Layout B parsing.

If queue is not draining:
- Audit `RadioIF_bleDrainRxQueue` (line 2386) — confirm it's actually called every event from `BleConnMgr_poll`. If not, fix the call site.

If ATT request never queued:
- Audit `att_client.c::AttClient_poll` and `AttClient_sendReadByGroupType` (or equivalent). Confirm `TXQueue_insert` is reached with `len > 0`.

Slave silent on the wire:
- Read CH573 BLE 4.2 stack docs about ATT MTU default and read-by-group-type response format. May need to pre-send `ATT_EXCHANGE_MTU_REQ`.

- [ ] **Step 2: Build, flash, retest with the same Task 1 Step 5 script**

- [ ] **Step 3: Iterate — but cap at 3 fix attempts**

Per systematic-debugging Phase 4 step 5: if 3 distinct fixes fail, **stop** and discuss architecture with the user before attempting #4.

---

## Task 4 — Validate gatt_discover + regression

- [ ] **Step 1: Full GATT round-trip with read**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
timeout 20 .venv/bin/python examples/lab/demo_ble_connect_gatt.py DC:32:62:8D:E1:09 0 --read 2>&1 | tee /tmp/f8a-s6-final.txt
```

Expected: connect → discovery returns ≥ 1 service → Device Name read returns "PwnPet_C81F" or whatever CH573 advertises → disconnect clean.

- [ ] **Step 2: Repeat 2× without reset between runs**

Per F8 exit criterion: discovery must succeed twice in a row from the same firmware load.

- [ ] **Step 3: Smoke regression — BLE 1M scan still works**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
timeout 8 .venv/bin/python -c "
from feralrf import Radio
r = Radio('/dev/ttyACM8'); r.connect(); r.init()
r.start_ble_scan(channel=37, scan_window=2.0)
import time; time.sleep(2.5)
pkts = r.get_packets()
print(f'BLE scan: {len(pkts)} packets')
r.disconnect()
"
```

Expected: ≥ 50 packets in 2 s (CH573 advertising at 100 ms interval = 20 packets/s, plus other ambient devices).

- [ ] **Step 4: 8 PHYs validation matrix smoke (best-effort)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
timeout 90 .venv/bin/pytest tests/test_validation_matrix.py -v 2>&1 | tail -30
```

Anchor change in Session 5 should be central-only; this should still PASS. If it fails, escalate before merging.

---

## Task 5 — Close-out

- [ ] **Step 1: Write Session 6 close-out doc**

Path: `docs/investigations/2026-04-24-f8a-session-1/session-6-closeout.md`

Mirror the Session 5 close-out structure. Sections: outcome, what happened, commits, evidence summary, what's permanent (the Task 2 instrumentation lands as permanent debug surface even if Task 1 alone fixed it), open items.

- [ ] **Step 2: Update F8A status in the design spec**

Edit `docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md` § F8A — change `🔜 (BLOQUEA F8)` to `✅`. Update F8 status from `🟡 BLOQUEADA por F8A` to `🟡` (still pending checkpoint, no longer blocked).

- [ ] **Step 3: Final commit (combines all of Task 2/3 if instrumentation landed) and tag**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --all-files
git add -- firmware/cc1352 python/feralrf docs
git status   # review
git commit -m "$(cat <<'EOF'
feat(f8a): unblock GATT — drain empty PDUs from master RX queue + diagnostics

Task 2 instrumentation (CMD_DEBUG_RX_QUEUE / CMD_DEBUG_TX_QUEUE) and
Task 5 docs land alongside the fix.

Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
EOF
)"

git tag -a v2.0-f8a -m "F8A: BLE central + GATT discovery green against CH573"
```

Do **not** push. The user pushes manually after a final review.

- [ ] **Step 4: Update `MEMORY.md` — replace `project_f8a_session5.md` reference with a new `project_f8a_done.md`**

Save a memory file: F8A done; GATT round-trip works; root cause was 3-entry RX queue saturated by empty PDUs (or whatever Task 2 surfaced). Remove the "what's still open" memory and add a "F8 unblocked, awaits human checkpoint" pointer.

---

## Risk register

- **Pre-commit could rewrite files unexpectedly.** Always inspect `git status` after `pre-commit run` and re-stage if needed.
- **Async RF err 0x2F on first init().** Carry-over from Session 3. If it now blocks Task 1 Step 1, defer cleanup to Session 7 and proceed by retrying init.
- **CH573 supervisionTimeout can drop the connection mid-test if Task 4 takes too long.** Each test script disconnects + reconnects per run.
- **`processBlePackets` Layout A vs Layout B parsing was tuned for advertising.** If Task 2 surfaces a length-mask bug for data PDUs, the fix is small but needs careful re-validation against the BLE scanner path (which uses the same function).

## Self-review checklist

- ✅ Goal explicit, exit criterion named.
- ✅ Hypothesis stated with falsification criterion.
- ✅ Smallest possible test first (1-line change), instrumentation as fallback only if hypothesis falsified.
- ✅ Exact file paths and line numbers throughout.
- ✅ All commands have expected outputs.
- ✅ Decision gates between phases — no blind execution.
- ✅ `pre-commit run` per `feedback_precommit` memory.
- ✅ Flash retry per `feedback_flash_retry`.
- ✅ Flash uses .hex per `feedback_flash_hex`.
- ✅ Tag only after exit criterion met (no premature `v2.0-f8a`).
- ✅ Cap at 3 fix attempts per systematic-debugging.
