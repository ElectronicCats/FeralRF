# F8A Session 1 — CONNECT_IND builder + first anchor (implementation plan)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a CONNECT_IND PDU that FeralRF constructs in-house (byte-for-byte known), put it on the wire at a timing we fully own, and capture a RAT timestamp of our own TX that downstream code can trust as `connTime`. This removes one half of the unknown that caused `BLE_DONE_NOSYNC` at the first master event. First master event timing (and sustained connection) is Session 2.

**Architecture:** Today `BleConn_initiate()` delegates both scan and CONNECT_IND TX to `CMD_BLE5_INITIATOR`, and `connTime` comes back as `Ble5_0_cmdBle5Initiator.pParams->connectTime` — an SDK-managed field whose exact semantics under `bDynamicWinOffset=1` are not documented clearly enough to debug this failure. Sniffle reports reliable connections using the same SDK command on the same hardware, so Session 1 first reproduces Sniffle's baseline locally, then determines whether the gap is a parameter-level fix or a true rewrite to manual scan + timed TX. We keep an investigation gate before touching code: **Task 5 decides the actual TX mechanism**. Session 1 ends with CONNECT_IND on the wire and a known `connTime` captured by us.

**Tech Stack:** TI-RTOS 7 (SysBIOS), CC1352P7, SimpleLink CC13xx/CC26xx SDK 8.30.01.01, C11. Validated with catnip flashing `.hex` (decision #17), two CatSniffer boards: one running FeralRF firmware under test (IEEE `00:12:4B:00:2A:79:BF:F1`), one running Sniffle as over-the-air oracle. Target peer: CH573 at `DC:32:62:8D:E1:09` (public, conn interval 30 ms).

**Branch:** `feature/f8a-ble-central-sniffle`, branched from `feature/f8-gatt-validation` at HEAD `8422cc4`.

**Session 1 definition of done:**
- Branch exists, all existing regressions still green (BLE scan, Sub-1GHz TX/RX, IEEE markers) flashed and smoke-tested.
- `BleConnMgr_poll()` runs in RfTask (re-apply f125473 cleanly). UART does not starve during connection attempts.
- CONNECT_IND PDU builder lives in its own file, byte-identical to a known-good reference, with a Python contract test pinning the byte layout.
- Our CONNECT_IND is observed on the wire by a second CatSniffer running Sniffle, carrying the AdvA/InitA/LLData fields we put in it.
- A captured RAT timestamp of our TX is stored on `BleConn_State.connTime` and exposed via `conn_status`.

**Out of scope for Session 1 (pushed to Session 2):** first MASTER event, sustained connection, LL control PDU round-trip, GATT. If the session finishes early and Session 2 entry tasks are well-defined, the executor may start Session 2 tasks — but **not** at the cost of un-validated Session 1 output.

---

## File structure

| File | Responsibility | Action |
|------|----------------|--------|
| `firmware/cc1352/src/main_rtos.c` | Task topology | Modify: move `BleConnMgr_poll()` call from UartTask to RfTask (re-apply f125473 diff). |
| `firmware/cc1352/src/ble_conn_pdu.c` | CONNECT_IND PDU encoder, pure functions, no SDK deps | **Create** |
| `firmware/cc1352/src/ble_conn_pdu.h` | Header for the encoder + struct `BleConnIndFields` | **Create** |
| `firmware/cc1352/src/ble_conn.c` | Connection-state owner; prepares and triggers initiation | Modify: `ble_conn_build_ll_data()` delegates the 22-byte LLData subset to `ble_conn_pdu.c`; full 34-byte PDU assembled via new builder. |
| `firmware/cc1352/src/radio_if.c` | RF-driver glue | Modify: add `RadioIF_bleScanAndConnect(...)` (investigation-driven implementation in Task 6). Existing `RadioIF_bleInitiate()` kept in-place for rollback, deprecated in comments. |
| `firmware/cc1352/include/radio_if.h` | Public API of radio_if | Modify: declare new function. |
| `python/tests/test_connect_ind_pdu.py` | Contract test pinning the CONNECT_IND byte layout | **Create** — pure-Python reference encoder + `bytes()` equality check, **used also as oracle** for on-wire capture in Task 8. |
| `docs/investigations/2026-04-24-f8a-session-1/` | Session evidence | **Create directory**. Holds captures, decisions, Sniffle logs. |

No file moves out of `firmware/cc1352/src/` in this session — layered sub-folders (`ble/`, `radio/`, …) are deferred per master spec §3.5.

---

## Task 0: Branch setup

**Files:**
- None modified; git state only.

- [ ] **Step 1: Verify pre-flight state**

Run:
```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git branch --show-current
git log --oneline -3
git status --short
```
Expected:
- current branch: `feature/f8-gatt-validation`
- HEAD: `8422cc4 docs(f8a): add F8A spec — BLE central rewrite Sniffle-style`
- working tree clean

If anything diverges, stop and report — **do not** create the branch.

- [ ] **Step 2: Create the session branch**

Run:
```bash
git switch -c feature/f8a-ble-central-sniffle
```
Expected: `Switched to a new branch 'feature/f8a-ble-central-sniffle'`.

- [ ] **Step 3: Create the investigations directory**

Run:
```bash
mkdir -p docs/investigations/2026-04-24-f8a-session-1
```

- [ ] **Step 4: Commit the empty directory with a README placeholder**

Write `docs/investigations/2026-04-24-f8a-session-1/README.md`:

```markdown
# F8A Session 1 investigation notes

Collects evidence referenced by
`docs/superpowers/plans/2026-04-24-f8a-session-1-connect-ind-first-anchor.md`.

Files land here as Session 1 tasks execute:
- `sniffle-baseline-<date>.log` — Sniffle firmware on board `00:12:4B:00:2A:79:BF:F1`
  connecting to CH573 `DC:32:62:8D:E1:09` (establishes hardware baseline).
- `feralrf-connect-ind-capture-<date>.log` — second-board Sniffle capture of our
  own CONNECT_IND TX (Task 8 oracle).
- `tx-mechanism-decision.md` — output of Task 5.
```

Run:
```bash
git add docs/investigations/2026-04-24-f8a-session-1/README.md
git commit -m "docs(f8a): create Session 1 investigation directory"
```

---

## Task 1: Re-apply f125473 — move BleConnMgr_poll to RfTask

**Why this is the first code change:** the UART-starvation fix is independent of the CONNECT_IND rewrite, has its own validated provenance (commit `f125473` on `fix/uart-starvation-during-conn`), and unblocks any manual test that issues host commands while a connection is being attempted. Re-applying it cleanly on the new branch avoids dragging in the follow-up (`5b7325a` WinOffset sweep) which is explicitly out-of-scope.

**Files:**
- Modify: `firmware/cc1352/src/main_rtos.c:115-175`

- [ ] **Step 1: Read the upstream diff**

Run:
```bash
git show f125473 -- firmware/cc1352/src/main_rtos.c
```
Study the two hunks. There is exactly one change: `BleConnMgr_poll()` + `BleConnMgr_isRunning()` call moves from UartTask's `while(1)` body into RfTask's `while(1)` body, right after `DataTask_poll()`. The comment in UartTask is updated to reference Sniffle's RadioTask model.

- [ ] **Step 2: Apply the change by hand**

In `firmware/cc1352/src/main_rtos.c`:

**Remove** from UartTask (around line 129-133):
```c
        if (BleConnMgr_isRunning()) {
            BleConnMgr_poll();
        }
```

**Replace** UartTask's loop comment (around line 122) with:
```c
    /* UART polling loop.
     *
     * BleConnMgr_poll() used to live here too, but Task_sleep() inside
     * it starved HostIFTask_poll() during live BLE connections. It now
     * runs in RfTask, aligned with Sniffle's RadioTask model.
     */
```

**Add** into RfTask's `while(1)` body (currently just `DataTask_poll(); Task_yield();`):
```c
    while (1) {
        DataTask_poll();

        /* Run BLE central connection events here (not in UartTask).
         * BleConnMgr_poll() sleeps up to one conn interval per event;
         * keeping it off UartTask prevents host-command starvation. */
        if (BleConnMgr_isRunning()) {
            BleConnMgr_poll();
        }

        Task_yield();
    }
```

- [ ] **Step 3: Build firmware**

Run:
```bash
cd firmware/cc1352 && rm -rf build && mkdir build && cd build
cmake .. && make -j$(nproc) 2>&1 | tail -20
```
Expected: clean build, `.hex` produced. No warnings introduced by the change. If warnings appear, stop and investigate.

- [ ] **Step 4: Flash and smoke-test**

Run catnip flash 2× before requesting a user reset (feedback memory: `flash_retry`):
```bash
catnip -p /dev/ttyACM<N> -f firmware/cc1352/build/feralrf_cc1352.hex --erase
```
Verify the device re-enumerates. Then from a Python shell:
```python
from feralrf import Radio
r = Radio("/dev/ttyACM<N>"); r.open(); print(r.get_info())
```
Expected: `RSP_INFO` with version byte, no timeout.

- [ ] **Step 5: Regression — BLE scan still works**

Run the existing BLE passive-scan demo for 5 s:
```bash
cd python && .venv/bin/python examples/lab/demo_ble_scanner.py --duration 5
```
Expected: ≥1 advertising packet (your phone will do). If zero packets and antenna is known good (CatSniffer BF:F1 has been scanning fine), stop — a task-move regression is possible.

- [ ] **Step 6: Commit**

```bash
git add firmware/cc1352/src/main_rtos.c
git commit -m "fix(f8a): move BleConnMgr_poll from UartTask to RfTask

Re-applies commit f125473 from fix/uart-starvation-during-conn onto
the Session 1 branch. UART polling no longer starves during active BLE
connections — BleConnMgr_poll() now runs alongside DataTask_poll()
in RfTask, matching Sniffle's RadioTask model.

Does NOT include 5b7325a (WinOffset sweep); that path is deprecated by
the Sniffle-style rewrite landing later in this session."
```

---

## Task 2: Baseline — Sniffle firmware works on the target board

**Why now, before any rewrite:** the F8A spec asserts Sniffle firmware connects to CH573 on board `BF:F1`. Evidence lived in `/tmp/sniffle_ch573_initiator.txt` which may not survive reboot. Re-capturing the baseline under the *current* RF environment lets Task 5 reason about "what does Sniffle do differently" with a fresh data point. No code change — this is observation only.

**Files:**
- Create: `docs/investigations/2026-04-24-f8a-session-1/sniffle-baseline-<YYYYMMDD>.log`

- [ ] **Step 1: Flash Sniffle firmware to the test board**

Back up the FeralRF hex currently on the board (the board is already flashed; we only need the path): the current session branch `feature/f8a-ble-central-sniffle` hex lives at `firmware/cc1352/build/feralrf_cc1352.hex`. If a Sniffle `.hex` for this board is not already present locally, ask the user for its path rather than rebuilding — **do not** try to build Sniffle from source in this task; that is yak-shaving.

If the user provides the Sniffle hex at `<SNIFFLE_HEX>`:
```bash
catnip -p /dev/ttyACM<N> -f <SNIFFLE_HEX> --erase
```

- [ ] **Step 2: Run Sniffle initiator against CH573**

Exact invocation must match the prior successful capture. From the user-provided Sniffle host tool:
```bash
sniff_receiver.py -s /dev/ttyACM<N> -c <channel> -m DC:32:62:8D:E1:09 --initiate --verbose \
  > docs/investigations/2026-04-24-f8a-session-1/sniffle-baseline-$(date +%Y%m%d).log 2>&1
```
*(Adjust flag names to whatever the user's Sniffle fork uses.)*

- [ ] **Step 3: Verify connection observed**

The log must contain evidence of a sustained connection: channel hops visible, LL_FEATURE_REQ/RSP exchanged, ≥10 data channel events before termination. If the log does NOT show this, Sniffle is not a valid oracle on this board today — **halt F8A and re-plan**. This is a hard gate.

- [ ] **Step 4: Re-flash FeralRF and verify**

```bash
catnip -p /dev/ttyACM<N> -f firmware/cc1352/build/feralrf_cc1352.hex --erase
```
`get_info()` round-trip as in Task 1 Step 4.

- [ ] **Step 5: Commit the baseline log**

```bash
git add docs/investigations/2026-04-24-f8a-session-1/sniffle-baseline-*.log
git commit -m "docs(f8a): capture Sniffle baseline on target board

Sniffle firmware on board 00:12:4B:00:2A:79:BF:F1 connects to CH573
at DC:32:62:8D:E1:09 with sustained channel hops. Establishes that the
hardware + peer + RF environment are not the root cause of FeralRF's
NOSYNC, and provides an on-wire oracle for Task 8."
```

---

## Task 3: CONNECT_IND reference encoder in Python (TDD oracle)

**Why Python:** firmware has no C unit-test harness. Python pytest is wired up (`python/tests/`). A Python encoder pinned to the BLE 5.0 Vol 6 Part B §2.3.3.1 layout serves two purposes: (a) tests the intent before firmware is written, (b) is the byte-level oracle against which Task 8 will validate the on-wire capture.

**Files:**
- Create: `python/tests/test_connect_ind_pdu.py`

- [ ] **Step 1: Write the failing test**

`python/tests/test_connect_ind_pdu.py`:

```python
"""
FeralRF - CONNECT_IND PDU byte-layout contract test.

Spec: BLE 5.0 Vol 6 Part B §2.3.3.1.
    Header (2 B): LLID/PDU-type (1 B) + Length (1 B)
        Byte 0 bits [3:0] = 0b0101 (CONNECT_IND), bit 6 = TxAdd (InitA type),
                 bit 7 = RxAdd (AdvA type).
    Payload (34 B): InitA (6 B) || AdvA (6 B) || LLData (22 B)
    LLData layout:
        AA (4) || CRCInit (3) || WinSize (1) || WinOffset (2) ||
        Interval (2) || Latency (2) || Timeout (2) || ChM (5) || Hop|SCA (1)
"""

import struct

from feralrf.ble.connect_ind import (
    BleConnIndFields,
    build_connect_ind_pdu,
    build_ll_data,
)


def test_ll_data_layout_matches_spec():
    fields = BleConnIndFields(
        init_addr=b"\x01\xee\xdd\xcc\xbb\xaa",
        init_addr_random=True,
        adv_addr=b"\x09\xe1\x8d\x62\x32\xdc",
        adv_addr_random=False,
        access_addr=0xAF9A5C3E,
        crc_init=0x123456,
        win_size=3,
        win_offset=7,
        interval=24,   # 30 ms / 1.25 ms = 24
        latency=0,
        timeout=100,
        channel_map=b"\xFF\xFF\xFF\xFF\x1F",
        hop_increment=11,
        sca=0,
    )

    ll = build_ll_data(fields)

    assert len(ll) == 22
    assert ll[0:4] == struct.pack("<I", 0xAF9A5C3E)
    assert ll[4:7] == b"\x56\x34\x12"
    assert ll[7] == 3
    assert ll[8:10] == struct.pack("<H", 7)
    assert ll[10:12] == struct.pack("<H", 24)
    assert ll[12:14] == struct.pack("<H", 0)
    assert ll[14:16] == struct.pack("<H", 100)
    assert ll[16:21] == b"\xFF\xFF\xFF\xFF\x1F"
    assert ll[21] == 11  # SCA=0, hop in low 5 bits


def test_connect_ind_pdu_header_bits():
    fields = BleConnIndFields(
        init_addr=b"\x01\xee\xdd\xcc\xbb\xaa",
        init_addr_random=True,           # TxAdd = 1
        adv_addr=b"\x09\xe1\x8d\x62\x32\xdc",
        adv_addr_random=False,           # RxAdd = 0
        access_addr=0xAF9A5C3E, crc_init=0, win_size=3, win_offset=0,
        interval=24, latency=0, timeout=100,
        channel_map=b"\xFF\xFF\xFF\xFF\x1F", hop_increment=11, sca=0,
    )

    pdu = build_connect_ind_pdu(fields)

    # Header byte 0: PDU type in bits [3:0] = 0b0101, TxAdd bit 6 = 1, RxAdd bit 7 = 0
    assert (pdu[0] & 0x0F) == 0b0101
    assert (pdu[0] >> 6) & 0x01 == 1   # TxAdd
    assert (pdu[0] >> 7) & 0x01 == 0   # RxAdd
    # Header byte 1: payload length = 34 (6 InitA + 6 AdvA + 22 LLData)
    assert pdu[1] == 34
    assert len(pdu) == 36


def test_connect_ind_pdu_payload_order():
    fields = BleConnIndFields(
        init_addr=b"\x01\xee\xdd\xcc\xbb\xaa",
        init_addr_random=True,
        adv_addr=b"\x09\xe1\x8d\x62\x32\xdc",
        adv_addr_random=False,
        access_addr=0xAF9A5C3E, crc_init=0x123456, win_size=3, win_offset=7,
        interval=24, latency=0, timeout=100,
        channel_map=b"\xFF\xFF\xFF\xFF\x1F", hop_increment=11, sca=0,
    )

    pdu = build_connect_ind_pdu(fields)

    # Bytes 2..7 = InitA (little-endian MAC as transmitted — octet 0 first)
    assert pdu[2:8] == b"\x01\xee\xdd\xcc\xbb\xaa"
    # Bytes 8..13 = AdvA
    assert pdu[8:14] == b"\x09\xe1\x8d\x62\x32\xdc"
    # Bytes 14..35 = 22 B LLData, matches build_ll_data()
    assert pdu[14:36] == build_ll_data(fields)
```

- [ ] **Step 2: Run test — must fail with ImportError**

Run:
```bash
cd python && .venv/bin/pytest tests/test_connect_ind_pdu.py -v
```
Expected: `ModuleNotFoundError: No module named 'feralrf.ble'` — this confirms the test drives the design, not the other way round.

- [ ] **Step 3: Implement `feralrf.ble.connect_ind`**

Create `python/feralrf/ble/__init__.py` (empty) and `python/feralrf/ble/connect_ind.py`:

```python
"""
CONNECT_IND PDU builder (pure Python reference encoder).

Byte-for-byte reference for the firmware C encoder in
`firmware/cc1352/src/ble_conn_pdu.c`. Both must produce identical bytes
given identical input fields. This module is tested by
`python/tests/test_connect_ind_pdu.py` and used as the oracle when Task 8
validates the firmware's on-wire CONNECT_IND.

Spec: BLE 5.0 Vol 6 Part B §2.3.3.1.
"""

from __future__ import annotations

from dataclasses import dataclass
import struct


CONNECT_IND_PDU_TYPE = 0b0101


@dataclass(frozen=True)
class BleConnIndFields:
    init_addr: bytes             # 6 B, LE as transmitted (octet 0 first)
    init_addr_random: bool       # TxAdd
    adv_addr: bytes              # 6 B, LE as transmitted
    adv_addr_random: bool        # RxAdd
    access_addr: int             # 32-bit
    crc_init: int                # 24-bit
    win_size: int                # 1 B, units of 1.25 ms
    win_offset: int              # 2 B, units of 1.25 ms
    interval: int                # 2 B, units of 1.25 ms
    latency: int                 # 2 B
    timeout: int                 # 2 B, units of 10 ms
    channel_map: bytes           # 5 B
    hop_increment: int           # 5 bits
    sca: int                     # 3 bits

    def __post_init__(self) -> None:
        if len(self.init_addr) != 6:
            raise ValueError("init_addr must be 6 B")
        if len(self.adv_addr) != 6:
            raise ValueError("adv_addr must be 6 B")
        if len(self.channel_map) != 5:
            raise ValueError("channel_map must be 5 B")
        if not (0 <= self.hop_increment <= 0x1F):
            raise ValueError("hop_increment must fit in 5 bits")
        if not (0 <= self.sca <= 0x07):
            raise ValueError("sca must fit in 3 bits")


def build_ll_data(f: BleConnIndFields) -> bytes:
    buf = bytearray(22)
    struct.pack_into("<I", buf, 0, f.access_addr)
    buf[4] = f.crc_init & 0xFF
    buf[5] = (f.crc_init >> 8) & 0xFF
    buf[6] = (f.crc_init >> 16) & 0xFF
    buf[7] = f.win_size & 0xFF
    struct.pack_into("<H", buf, 8, f.win_offset)
    struct.pack_into("<H", buf, 10, f.interval)
    struct.pack_into("<H", buf, 12, f.latency)
    struct.pack_into("<H", buf, 14, f.timeout)
    buf[16:21] = f.channel_map
    buf[21] = (f.hop_increment & 0x1F) | ((f.sca & 0x07) << 5)
    return bytes(buf)


def build_connect_ind_pdu(f: BleConnIndFields) -> bytes:
    ll = build_ll_data(f)
    payload = f.init_addr + f.adv_addr + ll    # 6 + 6 + 22 = 34
    hdr0 = CONNECT_IND_PDU_TYPE & 0x0F
    if f.init_addr_random:
        hdr0 |= 1 << 6       # TxAdd
    if f.adv_addr_random:
        hdr0 |= 1 << 7       # RxAdd
    header = bytes([hdr0, len(payload)])
    return header + payload
```

- [ ] **Step 4: Run the test — must pass**

Run:
```bash
cd python && .venv/bin/pytest tests/test_connect_ind_pdu.py -v
```
Expected: all three tests PASS.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/ble/__init__.py python/feralrf/ble/connect_ind.py python/tests/test_connect_ind_pdu.py
git commit -m "test(f8a): add Python CONNECT_IND PDU reference encoder

Byte-layout contract per BLE 5.0 Vol 6 Part B §2.3.3.1. Serves as both
oracle for the firmware C encoder (Task 4) and validator for on-wire
capture (Task 8)."
```

---

## Task 4: C CONNECT_IND encoder — byte-identical to Python oracle

**Files:**
- Create: `firmware/cc1352/include/ble_conn_pdu.h`
- Create: `firmware/cc1352/src/ble_conn_pdu.c`

- [ ] **Step 1: Create the header**

`firmware/cc1352/include/ble_conn_pdu.h`:

```c
#ifndef FERALRF_BLE_CONN_PDU_H
#define FERALRF_BLE_CONN_PDU_H

#include <stdbool.h>
#include <stdint.h>

#define BLE_CONN_IND_PDU_TYPE   0x05u   /* bits [3:0] of header byte 0 */
#define BLE_CONN_IND_PAYLOAD_LEN 34u    /* 6 InitA + 6 AdvA + 22 LLData */
#define BLE_CONN_IND_PDU_LEN    36u     /* 2 header + 34 payload */
#define BLE_CONN_LL_DATA_LEN    22u

typedef struct {
    uint8_t  initAddr[6];       /* little-endian as transmitted */
    bool     initAddrRandom;    /* TxAdd */
    uint8_t  advAddr[6];
    bool     advAddrRandom;     /* RxAdd */
    uint32_t accessAddr;
    uint32_t crcInit;           /* 24-bit, lower bits only */
    uint8_t  winSize;           /* 1.25 ms units */
    uint16_t winOffset;         /* 1.25 ms units */
    uint16_t interval;          /* 1.25 ms units */
    uint16_t latency;
    uint16_t timeout;           /* 10 ms units */
    uint8_t  channelMap[5];
    uint8_t  hopIncrement;      /* 5 bits */
    uint8_t  sca;               /* 3 bits */
} BleConnIndFields;

/* Fills the 22-byte LLData portion. Returns number of bytes written (always 22). */
uint8_t BleConnPdu_buildLlData(const BleConnIndFields *f, uint8_t *out);

/* Fills the full 36-byte CONNECT_IND PDU (2 header + 34 payload). Returns 36. */
uint8_t BleConnPdu_build(const BleConnIndFields *f, uint8_t *out);

#endif /* FERALRF_BLE_CONN_PDU_H */
```

- [ ] **Step 2: Create the implementation**

`firmware/cc1352/src/ble_conn_pdu.c`:

```c
#include "ble_conn_pdu.h"

#include <string.h>

uint8_t BleConnPdu_buildLlData(const BleConnIndFields *f, uint8_t *out) {
    out[0] = (uint8_t)(f->accessAddr & 0xFFu);
    out[1] = (uint8_t)((f->accessAddr >> 8) & 0xFFu);
    out[2] = (uint8_t)((f->accessAddr >> 16) & 0xFFu);
    out[3] = (uint8_t)((f->accessAddr >> 24) & 0xFFu);
    out[4] = (uint8_t)(f->crcInit & 0xFFu);
    out[5] = (uint8_t)((f->crcInit >> 8) & 0xFFu);
    out[6] = (uint8_t)((f->crcInit >> 16) & 0xFFu);
    out[7] = f->winSize;
    out[8]  = (uint8_t)(f->winOffset & 0xFFu);
    out[9]  = (uint8_t)((f->winOffset >> 8) & 0xFFu);
    out[10] = (uint8_t)(f->interval & 0xFFu);
    out[11] = (uint8_t)((f->interval >> 8) & 0xFFu);
    out[12] = (uint8_t)(f->latency & 0xFFu);
    out[13] = (uint8_t)((f->latency >> 8) & 0xFFu);
    out[14] = (uint8_t)(f->timeout & 0xFFu);
    out[15] = (uint8_t)((f->timeout >> 8) & 0xFFu);
    memcpy(&out[16], f->channelMap, 5);
    out[21] = (uint8_t)((f->hopIncrement & 0x1Fu) | ((f->sca & 0x07u) << 5));
    return BLE_CONN_LL_DATA_LEN;
}

uint8_t BleConnPdu_build(const BleConnIndFields *f, uint8_t *out) {
    uint8_t hdr0 = BLE_CONN_IND_PDU_TYPE & 0x0Fu;
    if (f->initAddrRandom) {
        hdr0 |= (uint8_t)(1u << 6);
    }
    if (f->advAddrRandom) {
        hdr0 |= (uint8_t)(1u << 7);
    }
    out[0] = hdr0;
    out[1] = BLE_CONN_IND_PAYLOAD_LEN;
    memcpy(&out[2], f->initAddr, 6);
    memcpy(&out[8], f->advAddr, 6);
    BleConnPdu_buildLlData(f, &out[14]);
    return BLE_CONN_IND_PDU_LEN;
}
```

- [ ] **Step 3: Wire into the build**

Check `firmware/cc1352/CMakeLists.txt` for the source list. If sources are globbed (e.g. `file(GLOB ...)`) no action; otherwise add `src/ble_conn_pdu.c` to the existing `target_sources(...)` or `add_executable(...)` list.

- [ ] **Step 4: Build**

```bash
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -10
```
Expected: clean compile, no warnings.

- [ ] **Step 5: Commit**

```bash
git add firmware/cc1352/include/ble_conn_pdu.h firmware/cc1352/src/ble_conn_pdu.c firmware/cc1352/CMakeLists.txt
git commit -m "feat(f8a): add CONNECT_IND PDU C encoder

Byte-identical to the Python oracle in feralrf.ble.connect_ind.
No SDK dependencies; pure function over BleConnIndFields. Will be
invoked from ble_conn.c once the manual scan+TX flow lands (Task 6/7)."
```

---

## Task 5: Decide the TX mechanism — investigation gate

**Why this is a dedicated task:** the F8A spec names `CMD_BLE5_GENERIC_TX` but no such command exists in SDK 8.30 (`rf_ble_cmd.h` only defines `CMD_BLE5_ADV`, `CMD_BLE5_ADV_NC`, `CMD_BLE5_ADV_DIR`, `CMD_BLE5_ADV_SCAN` for connectable/non-connectable adv TX, plus `CMD_BLE5_TX_TEST`). All BLE5 TX commands fix the PDU type via the opcode and ignore arbitrary type bits in the data buffer. We must commit to one of three viable paths before writing scan/TX glue, otherwise Task 6 will burn a whole session on a dead end. **No code changes** — this task ends in a decision document.

**Files:**
- Create: `docs/investigations/2026-04-24-f8a-session-1/tx-mechanism-decision.md`

- [ ] **Step 1: Read the SDK options**

Read sections of `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01/source/ti/devices/cc13x2x7_cc26x2x7/driverlib/rf_ble_cmd.h`:
  - `CMD_BLE5_INITIATOR` (line 1207) — what we use today; opaque.
  - `CMD_BLE5_ADV` family (line 1393, 1455, 1517, 1579) — fixed PDU types, see `rfc_bleAdvPar_s` (line 1858).
  - `rfc_bleAdvPar_s::pDeviceAddress` and `advLen` — controls the InitA/AdvA/payload that the radio prepends/sends.

Read Sniffle's `RadioWrapper_initiate()` (line 645) and `RadioTask.c:454-468` to confirm exactly how Sniffle invokes CMD_BLE5_INITIATOR — note the `connectTime = RF_getCurrentTime() + 4000` seed and `bDynamicWinOffset = 1`.

- [ ] **Step 2: Capture the parameter delta vs FeralRF**

Open `firmware/cc1352/src/ble_conn.c` (lines 132-213) and `firmware/cc1352/src/radio_if.c:2227-2296`. For each `Ble5_0_cmdBle5Initiator.pParams->...` field, record FeralRF's value alongside Sniffle's value into a markdown table. Look specifically for: `connectReqLen`, `bStrictLenFilter`, `endTime`/`endTrigger`, `randomState`, `chSel`, `bDynamicWinOffset`, `bUseWhiteList`.

- [ ] **Step 3: Document the three options**

Write `docs/investigations/2026-04-24-f8a-session-1/tx-mechanism-decision.md` covering:

1. **Option A — Stay on `CMD_BLE5_INITIATOR`, fix parameters.**
   - Effort: low (parameter sweep).
   - Risk: connTime semantics still implicit, may not generalise to all peers.
   - Falsifiable test: align every field with Sniffle's value, re-run against CH573, capture status/connTime.
2. **Option B — Custom `CMD_BLE5_ADV_NC` + post-TX pivot.**
   - Effort: high (forge a CONNECT_IND-shaped payload via ADV_NC, then immediately re-arm `CMD_BLE5_MASTER`; PDU header byte is forced to ADV_NONCONN_IND by the command — likely incompatible).
   - Risk: peer rejects PDU type; ADV_NC TX-end timestamp may not be exposed.
3. **Option C — Sniffle-style scan via `CMD_BLE5_GENERIC_RX` then chain `CMD_BLE5_INITIATOR` immediately.**
   - Effort: medium.
   - Bet: by entering INITIATOR with the radio already locked to the right channel and an observed ADV_IND timestamp, the SDK's `bDynamicWinOffset` produces a correct `connectTime`.
   - Falsifiable test: same as Option A but with a controlled scan precondition.

End the document with a recommendation. **Default recommendation if data is inconclusive: Option A**, because it is the smallest delta from current FeralRF and the largest delta from current FeralRF vs. Sniffle still lives in parameter values, not in TX-command choice. **Stop and surface this decision to the user before proceeding** — Task 6 / 7 / 8 implementations diverge depending on the choice.

- [ ] **Step 4: Commit the decision**

```bash
git add docs/investigations/2026-04-24-f8a-session-1/tx-mechanism-decision.md
git commit -m "docs(f8a): TX-mechanism decision for CONNECT_IND

Records the three viable paths under SDK 8.30 (CMD_BLE5_GENERIC_TX
does not exist), the parameter delta vs Sniffle, and the recommended
path forward for Tasks 6-8."
```

- [ ] **Step 5: HALT — request user confirmation of the chosen path**

Print the recommended option and the reason. **Do not** continue to Task 6 until the user acknowledges.

---

## Task 6: Implement scan + CONNECT_IND TX (Option A path-of-least-divergence)

**Pre-condition:** Task 5 chose Option A. If Task 5 chose B or C, the executor must rewrite this task before proceeding (full code differs).

**Files:**
- Modify: `firmware/cc1352/src/ble_conn.c:57-113` — replace internal byte-pack with call into `BleConnPdu_buildLlData()`. Keep the existing `s_ll_data` buffer and `s_state` populate.
- Modify: `firmware/cc1352/src/ble_conn.c:132-213` — keep `BleConn_initiate()` entry point shape, drive the parameter alignment in Step 2.
- Modify: `firmware/cc1352/src/radio_if.c:2227-2296` — bring `Ble5_0_cmdBle5Initiator.pParams` field-for-field into agreement with Sniffle's `RadioWrapper_initiate()`, per the table from Task 5 Step 2.

- [ ] **Step 1: Replace LL-data byte packing with `BleConnPdu_buildLlData()`**

In `firmware/cc1352/src/ble_conn.c`:
- `#include "ble_conn_pdu.h"` near the existing includes.
- Inside `ble_conn_build_ll_data(uint16_t interval, uint16_t timeout)` (line 57), populate a local `BleConnIndFields fields` from `s_state` and the function args, then call `BleConnPdu_buildLlData(&fields, s_ll_data)`. Continue to mirror the chosen values into `s_state` exactly as today (`accessAddr`, `crcInit`, `channelMap`, `hopIncrement`, `winOffset`, `connInterval`, `supervTimeout`).

This is a refactor with no functional change yet — running the existing demo should behave identically. **Build** (`cd firmware/cc1352/build && make -j$(nproc)`), expect clean.

- [ ] **Step 2: Align CMD_BLE5_INITIATOR params with Sniffle**

For each row of the Task-5 delta table, edit `firmware/cc1352/src/ble_conn.c` (parameter setup) and `firmware/cc1352/src/radio_if.c:2255-2272` to match Sniffle's values. Likely changes (verify each before applying):

  - `Ble5_0_cmdBle5Initiator.pParams->endTrigger.triggerType = TRIG_NEVER;` and `endTime = 0;` (Sniffle uses `forever=true`).
  - Drop the 5-second `endTime = now + 20000000u;` from `RadioIF_bleInitiate()`.
  - `bUseWhiteList = 0` and pass peer addr via `pWhiteList` pointer (already done — confirm).
  - `bStrictLenFilter = 1` (already done — confirm).
  - `chSel = 1` (already done — confirm).
  - `randomState = 0` (already done — confirm).
  - `bDynamicWinOffset = 1` (already done — confirm).

Do not change `bDynamicWinOffset` to 0 unless Task 5 explicitly recommends it; that flag's removal is its own experiment.

- [ ] **Step 3: Build**

```bash
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -10
```
Expected: clean.

- [ ] **Step 4: Flash**

```bash
catnip -p /dev/ttyACM<N> -f firmware/cc1352/build/feralrf_cc1352.hex --erase
```
Retry once on failure. Re-run `get_info()` smoke test.

- [ ] **Step 5: Commit**

```bash
git add firmware/cc1352/src/ble_conn.c firmware/cc1352/src/radio_if.c
git commit -m "feat(f8a): align BLE5_INITIATOR params with Sniffle

LLData byte-packing now goes through BleConnPdu_buildLlData() so the
firmware uses the same encoder validated by tests. CMD_BLE5_INITIATOR
parameter fields aligned to Sniffle's values per the Task-5 decision
document — endTrigger TRIG_NEVER, endTime 0, etc."
```

---

## Task 7: Persist our authoritative `connTime` and surface it via `conn_status`

**Why:** Whichever TX path Task 5/6 takes, we want a single `BleConn_State.connTime` field whose origin is documented (RAT tick of CONNECT_IND TX-end). Today it is unconditionally set from `Ble5_0_cmdBle5Initiator.pParams->connectTime` (`ble_conn.c:200`). Session 1 keeps that source under Option A but adds a debug counter and surfaces the raw RAT tick in `RSP_CONN_STATUS` so Session 2 can correlate Master-event timing logs without rebuilding.

**Files:**
- Modify: `firmware/cc1352/src/ble_conn.h` — add `uint32_t connTime;` if missing (it's already there per `ble_conn_mgr.c:184`, confirm).
- Modify: `firmware/cc1352/src/command_processor.c` — extend `CMD_CONN_STATUS` response to include `connTime` (4 B little-endian) at the end of the payload, behind a comment marker. **If the existing host-side parser asserts a fixed length**, add the field at the end and bump the protocol response length (verify in `python/feralrf/_responses.py`).
- Modify: `python/feralrf/_responses.py` — extend `ConnStatus` dataclass + parser if response length changed.
- Modify: `python/tests/test_radio_strict_responses.py` — extend conn-status round-trip if such a test exists.

- [ ] **Step 1: Read `command_processor.c`'s CMD_CONN_STATUS handler**

Run:
```bash
grep -n "CMD_CONN_STATUS\|RSP_CONN_STATUS" firmware/cc1352/src/command_processor.c
```
Identify the exact byte layout of the current response and the call sites in Python.

- [ ] **Step 2: Add `connTime` (4 B LE) to the response**

Append `connTime` to the response payload. Write the four bytes via `(uint8_t)(connTime & 0xFF)`, `(uint8_t)((connTime >> 8) & 0xFF)`, etc., immediately before the response is COBS-framed.

- [ ] **Step 3: Update Python parser**

Open `python/feralrf/_responses.py`. Find the conn-status dataclass / parser and add `conn_time: int = 0`. Parse the trailing 4 B little-endian. Make the field tolerant of older firmware (default to 0 if the response is the previous shorter length) **only if** the older length will realistically appear — if the same Session 1 firmware always emits the new length, fail loudly on a short response.

- [ ] **Step 4: Run host-side tests**

```bash
cd python && .venv/bin/pytest tests/ -v
```
Expected: all green. The new field is exercised at minimum by `test_radio_strict_responses.py` if it covers conn-status; otherwise add an inline assertion in an existing test or skip if no host-side coverage of `conn_status` exists yet.

- [ ] **Step 5: Build firmware**

```bash
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -10
```

- [ ] **Step 6: Commit**

```bash
git add firmware/cc1352/src/command_processor.c python/feralrf/_responses.py python/tests/
git commit -m "feat(f8a): expose connTime RAT tick in CMD_CONN_STATUS

connTime is the RAT-tick origin of the connection anchor — the
authority Session 2 uses when scheduling MASTER events. Surfacing
it in CONN_STATUS lets the host correlate firmware-side anchor
math with on-wire captures during debugging."
```

---

## Task 8: Hardware checkpoint — sniff our own CONNECT_IND

**Files:**
- Create: `docs/investigations/2026-04-24-f8a-session-1/feralrf-connect-ind-capture-<YYYYMMDD>.log`

- [ ] **Step 1: Flash the Session 1 firmware**

```bash
catnip -p /dev/ttyACM<N_FERAL> -f firmware/cc1352/build/feralrf_cc1352.hex --erase
```

- [ ] **Step 2: Set up a second CatSniffer running Sniffle as oracle**

The user will supply the second board's `/dev/ttyACM<N_SNIFF>` and a Sniffle invocation that captures advertising channel 37 with MAC filter for the InitA we will use (`01:EE:DD:CC:BB:AA` from the test fixture, matching Python test data) — or simply unfiltered for 5 s.

- [ ] **Step 3: Trigger one connection attempt from FeralRF**

In a Python REPL or via `python/examples/lab/demo_ble_connect_gatt.py` (without `--read`), issue `ble_connect("DC:32:62:8D:E1:09", addr_type=0)` against CH573, while the Sniffle oracle is recording.

Expected outcome: regardless of whether Session 1 produces a sustained connection, **Sniffle must capture our CONNECT_IND PDU on the air**, with bytes that decode to:
  - PDU type 0b0101.
  - InitA = our random static address.
  - AdvA = `DC:32:62:8D:E1:09`.
  - LLData ⇔ `BleConnPdu_buildLlData(...)` for the parameters reported by `conn_status`.

- [ ] **Step 4: Validate against the Python oracle**

Save the captured PDU bytes (or its decoded fields) to `docs/investigations/2026-04-24-f8a-session-1/feralrf-connect-ind-capture-<YYYYMMDD>.log`. Then in a Python REPL:

```python
from feralrf.ble.connect_ind import BleConnIndFields, build_connect_ind_pdu
# Plug in the connTime/AA/CRCInit/etc. reported by conn_status, plus the
# WinOffset/Interval/Timeout/ChM/Hop you set, then:
pdu = build_connect_ind_pdu(BleConnIndFields(...))
assert pdu == captured_pdu_bytes
```

If equal: Session 1's "CONNECT_IND on the wire and known by us" goal is met.
If not equal: **stop**, log the diff in the investigation directory, and **do not** open Session 2 — diagnose first.

- [ ] **Step 5: Commit the capture**

```bash
git add docs/investigations/2026-04-24-f8a-session-1/feralrf-connect-ind-capture-*.log
git commit -m "docs(f8a): capture FeralRF-emitted CONNECT_IND on air

Second-board Sniffle oracle decodes our CONNECT_IND byte-identical to
the Python reference encoder (feralrf.ble.connect_ind) for the
parameters reported by conn_status. Closes Session 1's CONNECT_IND
goal independent of whether the resulting connection sustains."
```

- [ ] **Step 6: Pre-commit + CI sanity**

Run pre-commit on the whole branch (feedback memory: never `--no-verify`):
```bash
pre-commit run --all-files
```
Run host tests:
```bash
cd python && .venv/bin/pytest -q
```
Both must be green before the session is declared done.

- [ ] **Step 7: Session 1 close-out report**

Print to the user, **do not** auto-merge:
1. Which TX-mechanism Option (A/B/C) was chosen.
2. The exact `connTime`, `accessAddr`, `crcInit`, `winOffset`, `interval`, `supervTimeout` from a representative connection attempt.
3. Whether the captured PDU matched the Python oracle.
4. Whether `conn_status` reports `connected=True` after the attempt — and if `events>0`, this means Session 2 is partially landed for free.

---

## Self-review — performed before handoff

- [x] Spec coverage: Session 1 covers F8A spec items "Replace CMD_BLE5_INITIATOR with manual CONNECT_IND" (gated by Task 5 — falls back to parameter alignment if manual TX is infeasible under SDK 8.30) and "Move BleConnMgr_poll to RfTask" (Task 1). The retire-ICall cleanup item is intentionally deferred to Session 3.
- [x] No placeholders.
- [x] Type consistency: `BleConnIndFields` (Python) ≡ `BleConnIndFields` (C) field-for-field. `BleConnPdu_buildLlData()` C signature returns `uint8_t` byte-count; Python `build_ll_data()` returns `bytes` of length 22. Both contracts are tested.
- [x] Realism check: Task 5's gate is the safety valve. If `CMD_BLE5_GENERIC_TX` had existed, Session 1 would have been a clean rewrite; it does not, so we converge on a parameter-alignment approach plus per-byte ownership of the LLData encoder.

---

## Sessions 2 and 3 — outline only

> Locked in only after Session 1 closes with the report above. Times are upper bounds, sized for a single sitting.

### Session 2 — First master event + sustained connection (~4-5 h)

**Entry condition:** Session 1 closed, CONNECT_IND-on-wire validated, FeralRF reports `connected=True` after `ble_connect` (whether or not events>0).

**Tasks (placeholder titles, real plan written before Session 2 begins):**
1. Telemetry: dump `BLE_DONE_*` status, RAT timestamp at MASTER startTime, RAT timestamp at first RX after MASTER returns. Push as a debug response (`RSP_DEBUG_TIMING`) so the host can plot anchor drift.
2. Empirical anchor sweep against CH573: vary `s_next_hop_time` initial offset by ±0…±5 connection intervals around `connTime + interval`, pick the offset that produces `BLE_DONE_OK` reliably. Persist as a constant.
3. LL control round-trip: confirm `LL_FEATURE_REQ` + `LL_VERSION_IND` exchange works through `handle_ll_ctrl()` over the new path (already implemented in `ble_conn_mgr.c:56-100`, only needs verification).
4. Stability: 60-second connection with `peripheralLatency=0`, no GATT traffic — log `events`, `tx`, `rx`, supervision-timeout fires.
5. Regression: `demo_ble_scanner.py` 5 s, BLE active scan, IEEE 802.15.4 markers OTA — 8/8 PHYs smoke.

**Exit:** `conn_status` shows `connected=True events>0 tx>0 rx>0 last_status=0x1400` for ≥10 events on the same connection.

### Session 3 — GATT validation + ICall cleanup (~3-4 h)

**Entry condition:** Session 2 closed, ≥60 s sustained connection.

**Tasks (placeholder titles):**
1. Run `python/examples/lab/demo_ble_connect_gatt.py DC:32:62:8D:E1:09 0 --read` end-to-end. Discovery returns ≥1 service + ≥1 characteristic. Read of Device Name returns expected string.
2. Disconnect / reconnect cycle: `CMD_DISCONNECT` then `CMD_CONNECT` to same target without firmware reset. `att_state` returns to IDLE.
3. Delete ICall vestiges: `firmware/cc1352/startup/osal_icall_ble.c`, `firmware/cc1352/syscfg/ti_ble_config.c/h`. Drop `#if 0 ICall_init()` block from `main_rtos.c`. Drop ICall blocks from `firmware/cc1352/include/config.h`.
4. Regression sweep: full validation matrix as in F6 (8/8 PHYs OTA markers 10/10), plus the F8 hardware integration test `python/tests/test_gatt_integration.py -m hardware`.
5. Commit + tag `v2.0-f8a` + close-out report. Unblocks F8 T12 checkpoint humano.

**Exit:** F8 closure criteria from master spec §5 are met → tag `v2.0-f8a` → request user approval to merge into `feature/f8-gatt-validation`.

---

## Risks and escape hatches

| Risk | Mitigation in Session 1 |
|------|-------------------------|
| Sniffle baseline (Task 2) does not reproduce on board `BF:F1` today | Hard halt — F8A's premise (Sniffle works, FeralRF doesn't) collapses; re-plan F8A or revisit board. |
| `CMD_BLE5_GENERIC_TX` truly doesn't exist (confirmed by SDK header read) and Option B/C also infeasible | Task 5 falls through to Option A (parameter alignment). Plan does not depend on rewriting the TX command. |
| Param alignment (Option A) does not fix NOSYNC | Session 1 still produces the CONNECT_IND encoder, the Python oracle, the UART-starvation fix, and the on-wire capture proof. Session 2's first task becomes "introduce telemetry to find the actual `connTime` semantics" with the investigation directory already populated. |
| `git switch -c` fails because the branch already exists | Stop and report. Do not force-overwrite. |
| Pre-commit fails on commit | Investigate per memory `feedback_precommit.md`; never `--no-verify`. |
| Flash repeatedly fails | Retry 2× per `feedback_flash_retry.md`, then ask the user to power-cycle. Use `.hex` only (decision #17). |

---

## Execution handoff

Plan complete and saved to `docs/superpowers/plans/2026-04-24-f8a-session-1-connect-ind-first-anchor.md`. Two execution options:

1. **Subagent-Driven (recommended)** — I dispatch a fresh subagent per task, review between tasks, fast iteration. Best fit here because Task 5 is an investigation gate that should be reviewed before Task 6 begins.
2. **Inline Execution** — Execute tasks in this session using `superpowers:executing-plans`. Suitable if you want to drive Task 2's hardware steps interactively without context switching.

**Which approach?** Once you choose, I will use `superpowers:subagent-driven-development` or `superpowers:executing-plans` accordingly.
