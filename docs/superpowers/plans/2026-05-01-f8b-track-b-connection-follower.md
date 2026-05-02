# F8b Track B — Sniffle-style Passive Connection Follower Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a capture-only mode that follows a non-FeralRF BLE central↔peripheral connection, hopping per CSA #2, and emits every captured LL data PDU as `RSP_LL_PACKET` to the host.

**Architecture:** New firmware module `ll_follower.c` owns a state machine `IDLE → SCAN_ADV → CONNECT_CAPTURED → FOLLOWING → IDLE`. It reuses FeralRF's existing `Ble5_0_cmdBle5GenericRx` primitive (already wired in `radio_if.c`) and the existing `csa2.c` (identical to Sniffle's). Two new `radio_if` helpers (`RadioIF_followAdvOnce`, `RadioIF_followDataOnce`) wrap one-shot blocking GenericRx with caller-supplied AA/CRC/endTime/MAC-filter. The follower polls from `ControlTask` — no new RTOS task. Host-side parser in pure Python (`_ll_parser.py`) decodes opcodes and exports pcap-NG.

**Tech Stack:** C99, TI-RTOS 7, TI SimpleLink CC13xx SDK 8.30, BLE5 RF Core (`Ble5_0_cmdBle5GenericRx`), Python 3, pyserial, dpkt for pcap-NG validation in tests.

**Scope guard — what is OUT:**
- SMP / pairing → Track C (separate plan)
- Notification reception via active connection → Track A (already shipped)
- Extended advertising (AUX_ADV_IND, periodic adv) → defer to F8c
- IRK-based RPA resolution → F8d
- Active forwarding/injection of captured packets → never (capture-only by spec)
- Bond/LTK persistence → F8d

**Closure bar (`v2.0-f8b-trackB`):**
1. Smoke test captures ≥10 bidirectional LL packets from a Phone↔Sony WH-CH720N pairing session.
2. pcap-NG opens cleanly in Wireshark with at least 1 ATT_WRITE_REQ visible.
3. ≥1 unit-test file passing 100% (≥15 cases, hardware-free).
4. Pre-commit clean across all modified files.
5. Firmware builds without new warnings.
6. No regression on F8b Track A smoke (`smoke_f8b_notifications.py` still 1+ notif/30s).

---

## File structure

**New firmware files:**
- `firmware/cc1352/include/ll_follower.h` — public API
- `firmware/cc1352/src/ll_follower.c` — state machine + connect-IND parser + per-event orchestration

**Modified firmware files:**
- `firmware/cc1352/include/radio_if.h` — declare two new helpers
- `firmware/cc1352/src/radio_if.c` — implement `RadioIF_followAdvOnce` + `RadioIF_followDataOnce`
- `firmware/cc1352/src/command_processor.c` — dispatch `CMD_FOLLOW_START`, `CMD_FOLLOW_STOP`; emit `RSP_LL_PACKET`/`RSP_FOLLOW_DONE` callbacks
- `firmware/cc1352/src/control_task.c` — call `LlFollower_poll()` each tick
- `firmware/cc1352/CMakeLists.txt` — add `src/ll_follower.c`

**New Python files:**
- `python/feralrf/_ll_parser.py` — pure-Python LL/ATT opcode decode + pcap-NG export
- `python/tests/test_ll_parser.py` — unit tests for parser + pcap export
- `python/tests/test_follow_connection.py` — unit tests for `Radio.follow_connection` API
- `python/examples/lab/smoke_f8b_follower.py` — hardware smoke against Sony+phone

**Modified Python files:**
- `python/feralrf/enums.py` — add `Command.FOLLOW_START`, `FOLLOW_STOP`, `Response.LL_PACKET`, `FOLLOW_DONE`
- `python/feralrf/commands.py` — `CommandBuilder.follow_start` / `follow_stop`
- `python/feralrf/radio.py` — add `LLPacket` dataclass, `follow_connection()`, `stop_follow_connection()`, `read_ll_packets()`

**Opcode allocations** (verified free vs. command_processor.c L24-99):
- `CMD_FOLLOW_START = 0x50`
- `CMD_FOLLOW_STOP  = 0x51`
- `RSP_LL_PACKET    = 0xAB`  (spec said 0x96 — collides with `RSP_AES`; using next free after Track A's 0xAA)
- `RSP_FOLLOW_DONE  = 0xAC`  (terminal status: peer disconnect / supervision timeout / host stop / sync failure)

Reserved (DO NOT use — Track C will claim):
- `CMD_PAIR = 0x52`, `CMD_PAIR_STATUS = 0x53`
- `RSP_PAIR_DONE = 0xAD`, `RSP_PAIR_STATUS = 0xAE`

---

## Pre-flight

### Task 0: Worktree + branch

**Files:** none

- [ ] **Step 1: Verify on `feature/ti-rtos-migration` HEAD = `15a4551`**

Run: `git rev-parse HEAD && git status --short`
Expected: `15a4551...` and only `M firmware/cc1352/include/radio_if.h` (the persistent unrelated WIP — leave unstaged per memory rule).

- [ ] **Step 2: Create branch `feature/f8b-track-b`**

Run: `git checkout -b feature/f8b-track-b`
Expected: `Switched to a new branch 'feature/f8b-track-b'`

- [ ] **Step 3: Build baseline so we know the tree is clean before any edits**

Run:
```
cd firmware/cc1352 && mkdir -p build && cd build && cmake .. >/dev/null && make -j$(nproc) 2>&1 | tail -20
```
Expected: build succeeds, no errors. Note build-time. Return to repo root: `cd ../../..`

---

## Phase 1 — Opcode + protocol scaffolding (host-side only, pure Python)

Doing Python first because it is hardware-free and TDD-friendly. Once Python knows the wire format, firmware just has to produce the right bytes.

### Task 1: Add opcodes to Python enums

**Files:**
- Modify: `python/feralrf/enums.py`

- [ ] **Step 1: Read existing enum to find insertion points**

Run: `grep -n "Command\.\|Response\." python/feralrf/enums.py | head -40`
Expected: see `Command` and `Response` IntEnum classes; locate the lines after `GATT_WRITE = 0x46` (Command) and after `RSP_GATT_NOTIFY` (Response).

- [ ] **Step 2: Append `FOLLOW_START`, `FOLLOW_STOP` to `Command` enum**

Add inside class `Command(IntEnum)`, after the existing GATT block (around line 84):
```python
    # F8b Track B — passive connection follower
    FOLLOW_START = 0x50
    FOLLOW_STOP = 0x51
```

- [ ] **Step 3: Append `LL_PACKET`, `FOLLOW_DONE` to `Response` enum**

Add inside class `Response(IntEnum)`, after the existing `RSP_ATT_DEBUG = 0xAA` line:
```python
    # F8b Track B — passive connection follower
    LL_PACKET = 0xAB
    FOLLOW_DONE = 0xAC
```

- [ ] **Step 4: Verify Python imports cleanly**

Run: `cd python && source .venv/bin/activate 2>/dev/null || true; python -c "from feralrf.enums import Command, Response; print(hex(Command.FOLLOW_START), hex(Response.LL_PACKET))" && cd ..`
Expected: `0x50 0xab`

- [ ] **Step 5: Commit**

```
git add python/feralrf/enums.py
git commit -m "feat(f8b-trackB): wire CMD_FOLLOW_START/STOP + RSP_LL_PACKET/FOLLOW_DONE opcodes"
```

### Task 2: CommandBuilder for follower

**Files:**
- Modify: `python/feralrf/commands.py`
- Test: `python/tests/test_follow_connection.py`

- [ ] **Step 1: Write failing test for `CommandBuilder.follow_start`**

Create `python/tests/test_follow_connection.py`:
```python
"""Unit tests for F8b Track B passive connection follower API."""
import struct
import pytest
from feralrf.commands import CommandBuilder


class TestFollowStartBuilder:
    def test_no_filter_yields_six_zero_bytes(self):
        # Wildcard MAC = b"\x00" * 6 (firmware contract: zero-MAC ⇒ no filter)
        out = CommandBuilder.follow_start(target_mac_le=None)
        assert out == b"\x00\x00\x00\x00\x00\x00"

    def test_explicit_mac_passed_through_little_endian(self):
        # MAC "AA:BB:CC:DD:EE:FF" → bytes A8 E6 E8 8A 7D F8 in BLE little-endian
        mac_le = bytes.fromhex("F87D8AE8E6A8")
        out = CommandBuilder.follow_start(target_mac_le=mac_le)
        assert out == mac_le
        assert len(out) == 6

    def test_wrong_length_raises(self):
        with pytest.raises(ValueError):
            CommandBuilder.follow_start(target_mac_le=b"\x01\x02\x03")  # len=3

    def test_seven_bytes_raises(self):
        with pytest.raises(ValueError):
            CommandBuilder.follow_start(target_mac_le=b"\x01" * 7)


class TestFollowStopBuilder:
    def test_empty_payload(self):
        assert CommandBuilder.follow_stop() == b""
```

- [ ] **Step 2: Run test, verify failure**

Run: `cd python && pytest tests/test_follow_connection.py::TestFollowStartBuilder -x -v 2>&1 | tail -15 && cd ..`
Expected: `AttributeError: type object 'CommandBuilder' has no attribute 'follow_start'`

- [ ] **Step 3: Implement `follow_start` and `follow_stop` in CommandBuilder**

Read `python/feralrf/commands.py` to find the class. After the existing `gatt_write` method, add:
```python
    @staticmethod
    def follow_start(target_mac_le: "bytes | None" = None) -> bytes:
        """Build CMD_FOLLOW_START payload.

        Args:
            target_mac_le: 6-byte MAC in BLE little-endian (LSB first), or
                None for wildcard (capture any CONNECT_IND seen).

        Wire format: [target_mac_le:6]  (all-zero ⇒ wildcard).
        """
        if target_mac_le is None:
            return b"\x00" * 6
        if len(target_mac_le) != 6:
            raise ValueError(
                f"target_mac_le must be exactly 6 bytes, got {len(target_mac_le)}"
            )
        return bytes(target_mac_le)

    @staticmethod
    def follow_stop() -> bytes:
        """Build CMD_FOLLOW_STOP payload (empty)."""
        return b""
```

- [ ] **Step 4: Run tests, verify pass**

Run: `cd python && pytest tests/test_follow_connection.py -x -v 2>&1 | tail -15 && cd ..`
Expected: 5 passed.

- [ ] **Step 5: Commit**

```
git add python/feralrf/commands.py python/tests/test_follow_connection.py
git commit -m "feat(f8b-trackB): CommandBuilder.follow_start/follow_stop + tests"
```

### Task 3: `LLPacket` dataclass

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_follow_connection.py`

- [ ] **Step 1: Write failing test for `LLPacket` dataclass shape**

Append to `python/tests/test_follow_connection.py`:
```python
class TestLLPacketDataclass:
    def test_construct_with_required_fields(self):
        from feralrf.radio import LLPacket
        pkt = LLPacket(
            direction="M->S",
            channel=10,
            rssi_dbm=-60,
            event_counter=42,
            payload=b"\x03\x05\x12\x00\x60\x01\x00",
            timestamp=1234.5,
        )
        assert pkt.direction == "M->S"
        assert pkt.channel == 10
        assert pkt.rssi_dbm == -60
        assert pkt.event_counter == 42
        assert pkt.payload == b"\x03\x05\x12\x00\x60\x01\x00"
        assert pkt.timestamp == 1234.5

    def test_direction_must_be_known_token(self):
        from feralrf.radio import LLPacket
        # Validation is deliberately *not* enforced in __init__; the firmware
        # is the source of truth. The host preserves whatever string it
        # was given. This test pins that the dataclass is permissive.
        pkt = LLPacket(
            direction="UNKNOWN",
            channel=0,
            rssi_dbm=0,
            event_counter=0,
            payload=b"",
            timestamp=0.0,
        )
        assert pkt.direction == "UNKNOWN"
```

- [ ] **Step 2: Run test, verify failure**

Run: `cd python && pytest tests/test_follow_connection.py::TestLLPacketDataclass -x -v 2>&1 | tail -10 && cd ..`
Expected: `ImportError: cannot import name 'LLPacket' from 'feralrf.radio'`

- [ ] **Step 3: Add `LLPacket` dataclass next to `GattNotification`**

Read `python/feralrf/radio.py` lines 119-131 to find `GattNotification`. Insert immediately after that dataclass (around line 132):
```python
@dataclass
class LLPacket:
    """A captured LL data PDU from a followed BLE connection.

    Emitted by the firmware's connection follower (CMD_FOLLOW_START) for
    every data-channel PDU it captures from a non-FeralRF central↔peripheral
    link. Capture-only — the firmware never injects on followed links.

    direction is "M->S" or "S->M" inferred from the LL header SN/NESN
    transitions, or "?" if the firmware could not determine direction.
    payload is the raw LL PDU starting at the 2-byte LL header.
    """

    direction: str
    channel: int
    rssi_dbm: int
    event_counter: int
    payload: bytes
    timestamp: float  # host monotonic at receive
```

- [ ] **Step 4: Run tests**

Run: `cd python && pytest tests/test_follow_connection.py -x -v 2>&1 | tail -15 && cd ..`
Expected: 7 passed.

- [ ] **Step 5: Commit**

```
git add python/feralrf/radio.py python/tests/test_follow_connection.py
git commit -m "feat(f8b-trackB): LLPacket dataclass + tests"
```

---

## Phase 2 — LL/ATT parser + pcap-NG export (pure Python, hardware-free)

This phase is fully testable without any firmware change. We build the host-side decoder so that once raw bytes arrive from the firmware, we can immediately produce useful output.

### Task 4: LL opcode parser (control PDUs)

**Files:**
- Create: `python/feralrf/_ll_parser.py`
- Test: `python/tests/test_ll_parser.py`

- [ ] **Step 1: Create test file with vector-based test cases**

Create `python/tests/test_ll_parser.py`:
```python
"""Unit tests for BLE LL / ATT PDU parser (host-side helper)."""
import pytest
from feralrf._ll_parser import parse_ll_pdu, LLPduKind, LL_OPCODE_NAMES


class TestParseLLData:
    def test_empty_data_pdu_llid1(self):
        # LL header: byte0 = LLID=01b (continuation/empty L2CAP), byte1 = length=0
        # Real packet: 01 00 (no payload)
        result = parse_ll_pdu(b"\x01\x00")
        assert result.kind == LLPduKind.DATA_CONT
        assert result.length == 0
        assert result.payload == b""

    def test_l2cap_start_llid2_with_att_write_req(self):
        # LLID=02 (L2CAP start), len=9, L2CAP[len:2 cid:2 att]
        # ATT_WRITE_REQ to handle 0x00d5 with value 01 00:
        # 02 09 05 00 04 00 12 d5 00 01 00
        raw = b"\x02\x09\x05\x00\x04\x00\x12\xd5\x00\x01\x00"
        result = parse_ll_pdu(raw)
        assert result.kind == LLPduKind.DATA_START
        assert result.length == 9
        assert result.payload == raw[2:]

    def test_ll_control_terminate_ind_llid3(self):
        # LLID=03 (LL control), len=2, opcode=0x02 (LL_TERMINATE_IND), reason=0x13
        # 03 02 02 13
        result = parse_ll_pdu(b"\x03\x02\x02\x13")
        assert result.kind == LLPduKind.CONTROL
        assert result.length == 2
        assert result.opcode == 0x02
        assert result.opcode_name == "LL_TERMINATE_IND"
        assert result.payload == b"\x02\x13"

    def test_ll_control_enc_req_llid3(self):
        # LL_ENC_REQ opcode=0x03, payload=22 bytes
        raw = bytes.fromhex("03160300010203040506070800010001020304050607080000")
        # 03=LLID3 16=len(22) 03=opcode then 22 body bytes (1 opcode + 21 fields)
        result = parse_ll_pdu(raw)
        assert result.kind == LLPduKind.CONTROL
        assert result.length == 0x16
        assert result.opcode == 0x03
        assert result.opcode_name == "LL_ENC_REQ"

    def test_unknown_ll_control_opcode(self):
        # LLID=03, len=1, opcode=0xFE (RFU)
        result = parse_ll_pdu(b"\x03\x01\xFE")
        assert result.opcode == 0xFE
        assert result.opcode_name.startswith("LL_RFU")

    def test_truncated_header_returns_none(self):
        assert parse_ll_pdu(b"") is None
        assert parse_ll_pdu(b"\x03") is None  # only header byte 0

    def test_truncated_payload_marked(self):
        # LLID=02, len=10, but only 5 payload bytes provided
        result = parse_ll_pdu(b"\x02\x0a\x01\x02\x03\x04\x05")
        assert result.truncated is True

    def test_llid_zero_is_reserved(self):
        # LLID=00 is reserved; parser should mark it but still return shape
        result = parse_ll_pdu(b"\x00\x00")
        assert result.kind == LLPduKind.RESERVED
```

- [ ] **Step 2: Run test, verify failure**

Run: `cd python && pytest tests/test_ll_parser.py -x -v 2>&1 | tail -10 && cd ..`
Expected: `ModuleNotFoundError: No module named 'feralrf._ll_parser'`

- [ ] **Step 3: Implement parser**

Create `python/feralrf/_ll_parser.py`:
```python
"""BLE LL / ATT PDU parser for the F8b Track B connection follower.

Decodes raw LL data PDUs (as captured by the firmware follower and emitted
via RSP_LL_PACKET) into structured form, and exports captures to pcap-NG
for Wireshark inspection. Pure Python, no firmware dependency.

References:
  - Bluetooth Core Spec 5.4 Vol 6 Part B §2.4 (Data Channel PDU)
  - Bluetooth Core Spec 5.4 Vol 3 Part F §3.4 (ATT Protocol)
"""
from dataclasses import dataclass
from enum import IntEnum
from typing import List, Optional


class LLPduKind(IntEnum):
    """LLID field of LL header (BT Core Spec Vol 6 Part B §2.4.2 Table 2.1)."""

    RESERVED = 0
    DATA_CONT = 1  # L2CAP continuation or empty PDU
    DATA_START = 2  # L2CAP start of complete frame
    CONTROL = 3  # LL Control PDU


# LL Control PDU opcodes (BT Core Spec Vol 6 Part B §2.4.2 Table 2.20)
LL_OPCODE_NAMES = {
    0x00: "LL_CONNECTION_UPDATE_IND",
    0x01: "LL_CHANNEL_MAP_IND",
    0x02: "LL_TERMINATE_IND",
    0x03: "LL_ENC_REQ",
    0x04: "LL_ENC_RSP",
    0x05: "LL_START_ENC_REQ",
    0x06: "LL_START_ENC_RSP",
    0x07: "LL_UNKNOWN_RSP",
    0x08: "LL_FEATURE_REQ",
    0x09: "LL_FEATURE_RSP",
    0x0A: "LL_PAUSE_ENC_REQ",
    0x0B: "LL_PAUSE_ENC_RSP",
    0x0C: "LL_VERSION_IND",
    0x0D: "LL_REJECT_IND",
    0x0E: "LL_SLAVE_FEATURE_REQ",
    0x0F: "LL_CONNECTION_PARAM_REQ",
    0x10: "LL_CONNECTION_PARAM_RSP",
    0x11: "LL_REJECT_EXT_IND",
    0x12: "LL_PING_REQ",
    0x13: "LL_PING_RSP",
    0x14: "LL_LENGTH_REQ",
    0x15: "LL_LENGTH_RSP",
    0x16: "LL_PHY_REQ",
    0x17: "LL_PHY_RSP",
    0x18: "LL_PHY_UPDATE_IND",
    0x19: "LL_MIN_USED_CHANNELS_IND",
}


@dataclass
class LLPdu:
    """Decoded LL Data Channel PDU."""

    kind: LLPduKind
    length: int
    payload: bytes  # bytes after the 2-byte LL header
    opcode: Optional[int] = None  # LL control opcode if kind==CONTROL
    opcode_name: Optional[str] = None
    truncated: bool = False  # length field exceeded available bytes


def parse_ll_pdu(raw: bytes) -> Optional[LLPdu]:
    """Decode an LL Data Channel PDU.

    Args:
        raw: bytes starting at the LL header (LLID byte). Does NOT include
            the access address or CRC — those are stripped by the firmware
            before emit.

    Returns:
        LLPdu, or None if the input is too short to even contain the header.
    """
    if len(raw) < 2:
        return None
    llid = raw[0] & 0x03
    length = raw[1]
    payload = raw[2 : 2 + length]
    truncated = len(payload) < length

    kind = LLPduKind(llid)
    pdu = LLPdu(kind=kind, length=length, payload=payload, truncated=truncated)

    if kind == LLPduKind.CONTROL and len(payload) >= 1:
        pdu.opcode = payload[0]
        pdu.opcode_name = LL_OPCODE_NAMES.get(
            pdu.opcode, f"LL_RFU_{pdu.opcode:#04x}"
        )

    return pdu
```

- [ ] **Step 4: Run tests, verify pass**

Run: `cd python && pytest tests/test_ll_parser.py -x -v 2>&1 | tail -20 && cd ..`
Expected: 8 passed.

- [ ] **Step 5: Commit**

```
git add python/feralrf/_ll_parser.py python/tests/test_ll_parser.py
git commit -m "feat(f8b-trackB): LL data PDU parser (LLID + control opcodes)"
```

### Task 5: ATT opcode decode helper

**Files:**
- Modify: `python/feralrf/_ll_parser.py`
- Test: `python/tests/test_ll_parser.py`

- [ ] **Step 1: Write failing test for ATT parser**

Append to `python/tests/test_ll_parser.py`:
```python
class TestParseATT:
    def test_att_write_req(self):
        from feralrf._ll_parser import parse_att_pdu, ATT_OPCODE_NAMES
        # ATT_WRITE_REQ to handle 0x00d5, value=01 00 (CCC notify enable)
        result = parse_att_pdu(b"\x12\xd5\x00\x01\x00")
        assert result.opcode == 0x12
        assert result.opcode_name == "ATT_WRITE_REQ"
        assert result.handle == 0x00d5
        assert result.value == b"\x01\x00"

    def test_att_handle_value_notification(self):
        from feralrf._ll_parser import parse_att_pdu
        # Handle 0x0064, value=AA BB CC
        result = parse_att_pdu(b"\x1b\x64\x00\xaa\xbb\xcc")
        assert result.opcode == 0x1B
        assert result.opcode_name == "ATT_HANDLE_VALUE_NTF"
        assert result.handle == 0x0064
        assert result.value == b"\xaa\xbb\xcc"

    def test_att_error_response(self):
        from feralrf._ll_parser import parse_att_pdu
        # ATT_ERROR_RSP: opcode=01, req_op=0x0a (READ), handle=0x0010, code=0x05 (insuf auth)
        result = parse_att_pdu(b"\x01\x0a\x10\x00\x05")
        assert result.opcode == 0x01
        assert result.opcode_name == "ATT_ERROR_RSP"

    def test_truncated_handle_returns_opcode_only(self):
        from feralrf._ll_parser import parse_att_pdu
        # Just an opcode, no handle bytes
        result = parse_att_pdu(b"\x12")
        assert result.opcode == 0x12
        assert result.handle is None

    def test_extract_att_from_l2cap_start(self):
        from feralrf._ll_parser import parse_ll_pdu, parse_att_pdu
        # LL data PDU containing a complete L2CAP frame with ATT_WRITE_REQ
        raw = b"\x02\x09\x05\x00\x04\x00\x12\xd5\x00\x01\x00"
        ll = parse_ll_pdu(raw)
        # L2CAP header is first 4 bytes of payload: [len:2][cid:2]
        att_bytes = ll.payload[4:]
        att = parse_att_pdu(att_bytes)
        assert att.opcode_name == "ATT_WRITE_REQ"
        assert att.handle == 0x00d5
```

- [ ] **Step 2: Run test, verify failure**

Run: `cd python && pytest tests/test_ll_parser.py::TestParseATT -x -v 2>&1 | tail -10 && cd ..`
Expected: `ImportError: cannot import name 'parse_att_pdu' from 'feralrf._ll_parser'`

- [ ] **Step 3: Add ATT parser to `_ll_parser.py`**

Append to `python/feralrf/_ll_parser.py`:
```python
# ATT opcodes (BT Core Spec Vol 3 Part F §3.4.8 Table 3.37)
ATT_OPCODE_NAMES = {
    0x01: "ATT_ERROR_RSP",
    0x02: "ATT_EXCHANGE_MTU_REQ",
    0x03: "ATT_EXCHANGE_MTU_RSP",
    0x04: "ATT_FIND_INFO_REQ",
    0x05: "ATT_FIND_INFO_RSP",
    0x06: "ATT_FIND_BY_TYPE_VALUE_REQ",
    0x07: "ATT_FIND_BY_TYPE_VALUE_RSP",
    0x08: "ATT_READ_BY_TYPE_REQ",
    0x09: "ATT_READ_BY_TYPE_RSP",
    0x0A: "ATT_READ_REQ",
    0x0B: "ATT_READ_RSP",
    0x0C: "ATT_READ_BLOB_REQ",
    0x0D: "ATT_READ_BLOB_RSP",
    0x0E: "ATT_READ_MULTIPLE_REQ",
    0x0F: "ATT_READ_MULTIPLE_RSP",
    0x10: "ATT_READ_BY_GROUP_TYPE_REQ",
    0x11: "ATT_READ_BY_GROUP_TYPE_RSP",
    0x12: "ATT_WRITE_REQ",
    0x13: "ATT_WRITE_RSP",
    0x16: "ATT_PREPARE_WRITE_REQ",
    0x17: "ATT_PREPARE_WRITE_RSP",
    0x18: "ATT_EXECUTE_WRITE_REQ",
    0x19: "ATT_EXECUTE_WRITE_RSP",
    0x1B: "ATT_HANDLE_VALUE_NTF",
    0x1D: "ATT_HANDLE_VALUE_IND",
    0x1E: "ATT_HANDLE_VALUE_CFM",
    0x52: "ATT_WRITE_CMD",
    0xD2: "ATT_SIGNED_WRITE_CMD",
}


@dataclass
class AttPdu:
    """Decoded ATT Protocol PDU."""

    opcode: int
    opcode_name: str
    handle: Optional[int] = None
    value: Optional[bytes] = None


def parse_att_pdu(raw: bytes) -> Optional[AttPdu]:
    """Decode an ATT PDU.

    Args:
        raw: bytes starting at the ATT opcode (after L2CAP header is stripped).

    Returns:
        AttPdu with handle/value populated when the opcode has them, or None
        if the input is empty.
    """
    if len(raw) < 1:
        return None
    opcode = raw[0]
    name = ATT_OPCODE_NAMES.get(opcode, f"ATT_RFU_{opcode:#04x}")
    pdu = AttPdu(opcode=opcode, opcode_name=name)

    # Opcodes that carry [handle:2LE] right after the opcode byte
    has_handle = opcode in {0x0A, 0x0B, 0x0C, 0x0D, 0x12, 0x16, 0x1B, 0x1D, 0x52, 0xD2}
    if has_handle and len(raw) >= 3:
        pdu.handle = int.from_bytes(raw[1:3], "little")
        pdu.value = raw[3:] if len(raw) > 3 else b""
    return pdu
```

- [ ] **Step 4: Run tests**

Run: `cd python && pytest tests/test_ll_parser.py -x -v 2>&1 | tail -20 && cd ..`
Expected: 13 passed.

- [ ] **Step 5: Commit**

```
git add python/feralrf/_ll_parser.py python/tests/test_ll_parser.py
git commit -m "feat(f8b-trackB): ATT PDU parser with handle/value extraction"
```

### Task 6: pcap-NG export

**Files:**
- Modify: `python/feralrf/_ll_parser.py`
- Test: `python/tests/test_ll_parser.py`

- [ ] **Step 1: Write failing round-trip test**

Append to `python/tests/test_ll_parser.py`:
```python
class TestExportPcap:
    def test_export_creates_valid_pcapng(self, tmp_path):
        from feralrf._ll_parser import export_pcap
        from feralrf.radio import LLPacket

        pkts = [
            LLPacket(
                direction="M->S",
                channel=10,
                rssi_dbm=-55,
                event_counter=1,
                payload=b"\x02\x09\x05\x00\x04\x00\x12\xd5\x00\x01\x00",
                timestamp=1700000000.0,
            ),
            LLPacket(
                direction="S->M",
                channel=10,
                rssi_dbm=-58,
                event_counter=1,
                payload=b"\x02\x05\x01\x00\x04\x00\x13",
                timestamp=1700000000.001,
            ),
        ]
        path = tmp_path / "f8b.pcapng"
        export_pcap(pkts, str(path))
        data = path.read_bytes()
        # pcap-NG starts with Section Header Block magic 0x0A0D0D0A
        assert data[:4] == b"\x0a\x0d\x0d\x0a"
        # Byte order magic 0x1A2B3C4D should appear in the section header
        assert b"\x4d\x3c\x2b\x1a" in data[:32]
        # Should contain at least 2 enhanced packet blocks (type 0x06)
        assert data.count(b"\x06\x00\x00\x00") >= 2

    def test_export_empty_list_creates_section_header_only(self, tmp_path):
        from feralrf._ll_parser import export_pcap
        path = tmp_path / "empty.pcapng"
        export_pcap([], str(path))
        data = path.read_bytes()
        assert data[:4] == b"\x0a\x0d\x0d\x0a"
        # No EPB
        assert data.count(b"\x06\x00\x00\x00") == 0
```

- [ ] **Step 2: Run test, verify failure**

Run: `cd python && pytest tests/test_ll_parser.py::TestExportPcap -x -v 2>&1 | tail -10 && cd ..`
Expected: `ImportError: cannot import name 'export_pcap'`

- [ ] **Step 3: Implement minimal pcap-NG writer**

Append to `python/feralrf/_ll_parser.py`:
```python
import struct
from typing import List


# Wireshark LINKTYPE_BLUETOOTH_LE_LL_WITH_PHDR = 256
# Pseudo-header: [rf_channel:1][signal_power:1][noise_power:1][access_addr_offset:1]
#                [ref_access_address:4][flags:2]
# We use the simpler LINKTYPE_BLUETOOTH_LE_LL = 251 which expects bare LL PDUs
# (without access address). Wireshark dissects it with the BLE LL dissector.
LINKTYPE_BLUETOOTH_LE_LL = 251


def _block(block_type: int, body: bytes) -> bytes:
    """Build a pcap-NG block with proper length and 4-byte alignment."""
    # Pad body to 4-byte boundary
    pad = (4 - (len(body) % 4)) % 4
    body_padded = body + (b"\x00" * pad)
    # Block: [type:4][len:4][body+pad:N][len:4]
    total_len = 12 + len(body_padded)
    return (
        struct.pack("<II", block_type, total_len)
        + body_padded
        + struct.pack("<I", total_len)
    )


def export_pcap(packets: List["LLPacket"], filename: str) -> None:
    """Write captured LL packets to a pcap-NG file.

    The file uses LINKTYPE_BLUETOOTH_LE_LL (251). Wireshark will dissect each
    packet's payload as a BLE LL PDU.

    Args:
        packets: list of LLPacket from Radio.read_ll_packets().
        filename: output path.
    """
    with open(filename, "wb") as f:
        # Section Header Block (type 0x0A0D0D0A)
        # Body: [byte_order_magic:4][major:2][minor:2][section_len:8]
        shb_body = struct.pack("<IHHq", 0x1A2B3C4D, 1, 0, -1)
        f.write(_block(0x0A0D0D0A, shb_body))

        # Interface Description Block (type 0x00000001)
        # Body: [linktype:2][reserved:2][snaplen:4]
        idb_body = struct.pack("<HHI", LINKTYPE_BLUETOOTH_LE_LL, 0, 65535)
        f.write(_block(0x00000001, idb_body))

        # Enhanced Packet Block per packet (type 0x00000006)
        # Body: [interface_id:4][ts_high:4][ts_low:4][cap_len:4][orig_len:4][data:N]
        for pkt in packets:
            ts_us = int(pkt.timestamp * 1_000_000)
            ts_high = (ts_us >> 32) & 0xFFFFFFFF
            ts_low = ts_us & 0xFFFFFFFF
            data = pkt.payload
            epb_body = struct.pack(
                "<IIIII", 0, ts_high, ts_low, len(data), len(data)
            ) + data
            f.write(_block(0x00000006, epb_body))
```

- [ ] **Step 4: Run all parser tests**

Run: `cd python && pytest tests/test_ll_parser.py -v 2>&1 | tail -20 && cd ..`
Expected: 15 passed.

- [ ] **Step 5: Commit**

```
git add python/feralrf/_ll_parser.py python/tests/test_ll_parser.py
git commit -m "feat(f8b-trackB): pcap-NG export with LINKTYPE_BLUETOOTH_LE_LL"
```

---

## Phase 3 — Firmware: radio_if helpers (CONNECT_IND-aware Generic RX)

We need two blocking primitives:
- `RadioIF_followAdvOnce(channel, end_time, packet_cb, user)` — listen on ch37/38/39 with AA=0x8E89BED6, CRC init=0x555555, until `end_time` or callback returns stop. Used to capture CONNECT_IND.
- `RadioIF_followDataOnce(channel, accessAddr, crcInit, end_time, packet_cb, user)` — listen on a data channel with the captured AA + CRCInit, until `end_time`. Used to follow each connection event.

Both wrap the existing `Ble5_0_cmdBle5GenericRx` (already proven on this hardware) plus the existing RF data queue plumbing.

### Task 7: Header for the two new helpers

**Files:**
- Modify: `firmware/cc1352/include/radio_if.h`

- [ ] **Step 1: Find insertion point in `radio_if.h`**

Read `firmware/cc1352/include/radio_if.h` lines 100-145 to find the BLE central section.

- [ ] **Step 2: Append new follower helpers near end (before `/* Jamming functions */`)**

Insert at line 138 (just before `/* Jamming functions */`):
```c
/* F8b Track B — Passive connection follower primitives.
 *
 * Both helpers wrap CMD_BLE5_GENERIC_RX as a blocking single-shot RX.
 * The packet callback is invoked from the same task context that called
 * the helper, after the RF command terminates and the data queue has been
 * drained. Callback signature receives the raw LL bytes (header + body),
 * channel, RSSI, and a user pointer for state.
 *
 * Returns 0 on RX_OK / TIMEOUT / END (normal terminations), non-zero on
 * RF stack error. */
typedef void (*RadioIF_FollowPacketCb)(const uint8_t *ll_pdu, uint8_t pdu_len,
                                       uint8_t channel, int8_t rssi_dbm, void *user);

/* Listen on an advertising channel (37/38/39) with AA=0x8E89BED6,
 * CRC init=0x555555. end_time_rat is absolute RAT tick (4 MHz); 0 means
 * listen forever. */
int RadioIF_followAdvOnce(uint8_t adv_channel, uint32_t end_time_rat,
                          RadioIF_FollowPacketCb cb, void *user);

/* Listen on a data channel (0..36) with caller-supplied accessAddr and
 * crcInit. end_time_rat is absolute RAT tick; 0 means listen forever. */
int RadioIF_followDataOnce(uint8_t data_channel, uint32_t accessAddr,
                           uint32_t crcInit, uint32_t end_time_rat,
                           RadioIF_FollowPacketCb cb, void *user);
```

- [ ] **Step 3: Verify header still parses**

Run:
```
cd firmware/cc1352/build && cmake --build . --target ll_manager.c.obj 2>&1 | tail -5; cd ../../..
```
Expected: no parse errors (this just compiles a translation unit that includes radio_if.h).

- [ ] **Step 4: DO NOT commit yet** — we'll commit alongside the implementation.

### Task 8: Implementation of `RadioIF_followAdvOnce` + `RadioIF_followDataOnce`

**Files:**
- Modify: `firmware/cc1352/src/radio_if.c`

- [ ] **Step 1: Find a placement near existing BLE central code**

Run: `grep -n "RadioIF_bleCentral\|RadioIF_bleResetRxQueue\|^int RadioIF_bleInitiate" firmware/cc1352/src/radio_if.c | head -10`
Note the line of `RadioIF_bleCentral`. Insert the new helpers immediately after that function.

- [ ] **Step 2: Add implementations**

Insert (right after the function body of `RadioIF_bleCentral` ends — find the matching `}`):
```c
/* ── F8b Track B — passive follower primitives ── */

#define BLE_ADV_AA 0x8E89BED6u
#define BLE_ADV_CRC_INIT 0x555555u

/* Drain the RF data queue and invoke the callback per packet.
 * This walks the dataEntry ring populated by Ble5_0_cmdBle5GenericRx,
 * matching the pattern in RadioIF_poll(). */
static void RadioIF_drainFollowQueue(RadioIF_FollowPacketCb cb, void *user,
                                     uint8_t channel) {
    while (RadioIF_rfHasPacket()) {
        rfc_dataEntryGeneral_t *entry =
            (rfc_dataEntryGeneral_t *)s_rf_data_queue.pCurrEntry;
        if (entry == NULL) {
            break;
        }
        if (entry->status == DATA_ENTRY_FINISHED) {
            uint8_t *data = (uint8_t *)&entry->data;
            /* GenericRx with bIncludeLenByte=1, bIncludeCrc=1, bAppendRssi=1,
             * bAppendStatus=1, bAppendTimestamp=1.
             * Layout: [len:1][LL hdr 2][LL body N][CRC:3][RSSI:1][status:1][ts:4]
             * The "len" byte is the LL PDU length INCLUDING header. */
            uint8_t total_len = data[0];
            uint8_t pdu_len = total_len; /* LL hdr + body */
            int8_t rssi = (int8_t)data[1 + total_len + 3]; /* skip len+pdu+CRC */

            if (cb != NULL && pdu_len >= 2u) {
                cb(&data[1], pdu_len, channel, rssi, user);
            }
        }
        RadioIF_rfConsumeEntry();
    }
}

int RadioIF_followAdvOnce(uint8_t adv_channel, uint32_t end_time_rat,
                          RadioIF_FollowPacketCb cb, void *user) {
    if (adv_channel < 37u || adv_channel > 39u) {
        return -1;
    }
    /* Lazy-init RF in BLE mode if the handle is not yet open in BLE
     * (matches F22.b lazy-open pattern). */
    if (s_rf_handle == NULL || s_rf_mode != RADIO_IF_RF_MODE_BLE) {
        if (!RadioIF_switchRfMode(&Ble5_0_mode, (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup)) {
            return -2;
        }
        s_rf_mode = RADIO_IF_RF_MODE_BLE;
    }

    RadioIF_resetRfDataQueue();

    /* Configure GenericRx for ADV channel scan */
    RadioIF_applyBleChannelConfig(adv_channel);
    Ble5_0_cmdBle5GenericRx.pParams->accessAddress = BLE_ADV_AA;
    Ble5_0_cmdBle5GenericRx.pParams->crcInit0 = (uint8_t)(BLE_ADV_CRC_INIT & 0xFFu);
    Ble5_0_cmdBle5GenericRx.pParams->crcInit1 = (uint8_t)((BLE_ADV_CRC_INIT >> 8) & 0xFFu);
    Ble5_0_cmdBle5GenericRx.pParams->crcInit2 = (uint8_t)((BLE_ADV_CRC_INIT >> 16) & 0xFFu);
    Ble5_0_cmdBle5GenericRx.pParams->endTrigger.triggerType =
        (end_time_rat == 0u) ? TRIG_NEVER : TRIG_ABSTIME;
    Ble5_0_cmdBle5GenericRx.pParams->endTime = end_time_rat;
    Ble5_0_cmdBle5GenericRx.pParams->pRxQ = &s_rf_data_queue;
    Ble5_0_cmdBle5GenericRx.startTrigger.triggerType = TRIG_NOW;

    /* Run command first; then drain. Polling-during-RX would race with the
     * data queue. RF_runCmd blocks until end trigger fires (or peer adv
     * completes if we cancel). */
    RF_EventMask events = RF_runCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdBle5GenericRx,
                                    RF_PriorityNormal, NULL,
                                    RF_EventLastCmdDone | RF_EventCmdAborted);
    (void)events;

    RadioIF_drainFollowQueue(cb, user, adv_channel);
    return (int)Ble5_0_cmdBle5GenericRx.status;
}

int RadioIF_followDataOnce(uint8_t data_channel, uint32_t accessAddr,
                           uint32_t crcInit, uint32_t end_time_rat,
                           RadioIF_FollowPacketCb cb, void *user) {
    if (data_channel > 36u) {
        return -1;
    }
    if (s_rf_handle == NULL || s_rf_mode != RADIO_IF_RF_MODE_BLE) {
        if (!RadioIF_switchRfMode(&Ble5_0_mode, (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup)) {
            return -2;
        }
        s_rf_mode = RADIO_IF_RF_MODE_BLE;
    }

    RadioIF_resetRfDataQueue();

    /* Data-channel whitening uses the BLE channel index (0..36 mapped to
     * RF channels 0..39 with 37/38/39 reserved for ADV). For data channels
     * we whitening init = (0x40 | data_channel). */
    Ble5_0_cmdBle5GenericRx.channel = data_channel;
    Ble5_0_cmdBle5GenericRx.whitening.init = (uint8_t)(0x40u | (data_channel & 0x3Fu));
    Ble5_0_cmdBle5GenericRx.whitening.bOverride = 1u;
    Ble5_0_cmdFs.frequency = RadioIF_bleChannelToFrequency(data_channel);
    Ble5_0_cmdFs.fractFreq = 0u;

    Ble5_0_cmdBle5GenericRx.pParams->accessAddress = accessAddr;
    Ble5_0_cmdBle5GenericRx.pParams->crcInit0 = (uint8_t)(crcInit & 0xFFu);
    Ble5_0_cmdBle5GenericRx.pParams->crcInit1 = (uint8_t)((crcInit >> 8) & 0xFFu);
    Ble5_0_cmdBle5GenericRx.pParams->crcInit2 = (uint8_t)((crcInit >> 16) & 0xFFu);
    Ble5_0_cmdBle5GenericRx.pParams->endTrigger.triggerType =
        (end_time_rat == 0u) ? TRIG_NEVER : TRIG_ABSTIME;
    Ble5_0_cmdBle5GenericRx.pParams->endTime = end_time_rat;
    Ble5_0_cmdBle5GenericRx.pParams->pRxQ = &s_rf_data_queue;
    Ble5_0_cmdBle5GenericRx.startTrigger.triggerType = TRIG_NOW;

    RF_EventMask events = RF_runCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdBle5GenericRx,
                                    RF_PriorityNormal, NULL,
                                    RF_EventLastCmdDone | RF_EventCmdAborted);
    (void)events;

    RadioIF_drainFollowQueue(cb, user, data_channel);
    return (int)Ble5_0_cmdBle5GenericRx.status;
}
```

- [ ] **Step 3: Build firmware**

Run:
```
cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -20; cd ../../..
```
Expected: build succeeds. If `RadioIF_rfHasPacket` / `RadioIF_rfConsumeEntry` / `s_rf_data_queue` are static and not visible at the new function location, move the new functions below the static helpers (search for `static bool RadioIF_rfHasPacket` and place the follower helpers after it).

- [ ] **Step 4: Commit (header + impl together)**

```
git add firmware/cc1352/include/radio_if.h firmware/cc1352/src/radio_if.c
git commit -m "feat(f8b-trackB): RadioIF_followAdvOnce + RadioIF_followDataOnce primitives"
```

---

## Phase 4 — Firmware: ll_follower module (state machine)

### Task 9: ll_follower header

**Files:**
- Create: `firmware/cc1352/include/ll_follower.h`

- [ ] **Step 1: Create header**

Create `firmware/cc1352/include/ll_follower.h`:
```c
/*
 * FeralRF CC1352 — Sniffle-style passive connection follower (F8b Track B).
 *
 * Captures a non-FeralRF central↔peripheral BLE connection without
 * participating. Hops per CSA #2 using the captured CONNECT_IND parameters.
 * Capture-only: never transmits on the followed link.
 *
 * State machine:
 *   IDLE
 *     │ LlFollower_start(target_mac)
 *     ▼
 *   SCAN_ADV — rotates ch37→ch38→ch39 with AA=0x8E89BED6,
 *              filters CONNECT_IND by InitA == target_mac (or wildcard).
 *     │ CONNECT_IND captured
 *     ▼
 *   FOLLOWING — for each conn event, computes channel via CSA #2,
 *               runs GenericRx with captured AA/CRCInit until next anchor.
 *     │ supervision timeout / LL_TERMINATE_IND / LlFollower_stop
 *     ▼
 *   IDLE
 */

#ifndef LL_FOLLOWER_H
#define LL_FOLLOWER_H

#include <stdbool.h>
#include <stdint.h>

#define LL_FOLLOWER_MAC_LEN 6u

/* Reasons reported in LlFollower_DoneInfo.reason */
typedef enum {
    LL_FOLLOWER_DONE_HOST_STOP = 0,    /* host called LlFollower_stop */
    LL_FOLLOWER_DONE_PEER_TERMINATE,   /* LL_TERMINATE_IND seen on link */
    LL_FOLLOWER_DONE_SUPERVISION,      /* > supervisionTimeout without RX */
    LL_FOLLOWER_DONE_SYNC_FAILED,      /* CONNECT_IND captured but >5 events with 0 packets */
    LL_FOLLOWER_DONE_CONNECT_TIMEOUT,  /* no CONNECT_IND for target within scan window */
} LlFollower_DoneReason;

typedef struct {
    uint8_t reason;
    uint32_t packets_captured;
} LlFollower_DoneInfo;

/* Callbacks the host application installs to receive captured packets and
 * the terminal "done" event. Both fire from the same task that calls
 * LlFollower_poll(). */
typedef void (*LlFollower_PacketCb)(const uint8_t *ll_pdu, uint8_t pdu_len,
                                    uint8_t channel, int8_t rssi_dbm,
                                    uint16_t event_counter, uint8_t direction);

typedef void (*LlFollower_DoneCb)(const LlFollower_DoneInfo *info);

typedef struct {
    LlFollower_PacketCb onPacket;
    LlFollower_DoneCb onDone;
} LlFollower_Callbacks;

void LlFollower_init(void);
void LlFollower_setCallbacks(const LlFollower_Callbacks *cb);

/* Start the follower. target_mac_le is the 6-byte LE MAC to filter on; pass
 * all-zero for wildcard. Returns false if already running. */
bool LlFollower_start(const uint8_t target_mac_le[LL_FOLLOWER_MAC_LEN]);
bool LlFollower_stop(void);
bool LlFollower_isRunning(void);

/* Poll once. Drives the state machine forward by one step (one ADV scan
 * burst, or one connection event). Should be called from the main task
 * loop while the follower is running. */
void LlFollower_poll(void);

#endif /* LL_FOLLOWER_H */
```

- [ ] **Step 2: Verify header parses**

Run: `gcc -c -fsyntax-only -Ifirmware/cc1352/include firmware/cc1352/include/ll_follower.h 2>&1 | head`
Expected: no output (header is self-contained).

- [ ] **Step 3: Don't commit yet** — paired with impl below.

### Task 10: ll_follower.c — IDLE + SCAN_ADV + CONNECT_IND parser

**Files:**
- Create: `firmware/cc1352/src/ll_follower.c`

- [ ] **Step 1: Create file with state machine + ADV scan loop**

Create `firmware/cc1352/src/ll_follower.c`:
```c
/*
 * FeralRF CC1352 — Sniffle-style passive connection follower.
 *
 * Reuses FeralRF's existing csa2.c (identical algorithm) and
 * radio_if's GenericRx-based primitives. Adapts the state-machine
 * pattern from Sniffle's RadioTask.c (BSD/GPLv3 by Sultan Qasim Khan,
 * NCC Group plc).
 */
#include "ll_follower.h"

#include "csa2.h"
#include "radio_if.h"

#include <ti/drivers/rf/RF.h>

#include <stdint.h>
#include <string.h>

/* ── State ── */

typedef enum {
    LL_FOLLOWER_STATE_IDLE = 0,
    LL_FOLLOWER_STATE_SCAN_ADV,
    LL_FOLLOWER_STATE_FOLLOWING,
} LlFollower_State;

/* CONNECT_IND LL data layout (BT Core Spec 5.4 Vol 6 Part B §2.3.3.1):
 *   AccessAddress:4  CRCInit:3  WinSize:1  WinOffset:2  Interval:2
 *   Latency:2  Timeout:2  ChM:5  Hop:5b+SCA:3b */
#define CONNECT_IND_LL_LEN 22u

static LlFollower_State s_state = LL_FOLLOWER_STATE_IDLE;
static uint8_t s_target_mac[LL_FOLLOWER_MAC_LEN];
static bool s_have_target;
static LlFollower_Callbacks s_cb;

/* Captured connection parameters */
static uint32_t s_access_addr;
static uint32_t s_crc_init;
static uint16_t s_hop_interval; /* 1.25 ms units */
static uint16_t s_supervision;  /* 10 ms units */
static uint64_t s_chan_map;
static uint8_t s_hop_increment;
static bool s_use_csa2;

/* Per-event state */
static uint16_t s_event_counter;
static uint32_t s_next_anchor_rat; /* RAT ticks (4 MHz) */
static uint32_t s_last_rx_rat;
static uint32_t s_packets_captured;
static uint8_t s_scan_channel; /* rotates 37→38→39 */
static uint8_t s_zero_rx_streak;

/* For ADV-channel scan callback marshalling */
static bool s_connect_ind_pending;

/* ── Helpers ── */

static uint32_t s_rd24_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static uint32_t s_rd32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t s_rd16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint64_t s_rd40_le(const uint8_t *p) {
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) | ((uint64_t)p[2] << 16) |
           ((uint64_t)p[3] << 24) | ((uint64_t)p[4] << 32);
}

static void s_emit_done(uint8_t reason) {
    if (s_cb.onDone) {
        LlFollower_DoneInfo info = {
            .reason = reason,
            .packets_captured = s_packets_captured,
        };
        s_cb.onDone(&info);
    }
    s_state = LL_FOLLOWER_STATE_IDLE;
}

/* Parse CONNECT_IND LLData (22 bytes after the 14-byte adv header) and
 * load the captured connection into module state. Returns true on success.
 */
static bool s_parse_connect_ind_lldata(const uint8_t *lldata) {
    s_access_addr = s_rd32_le(&lldata[0]);
    s_crc_init = s_rd24_le(&lldata[4]);
    /* WinSize at lldata[7], WinOffset at lldata[8..9] — we ignore both;
     * for capture-only we sync to first observed packet anchor. */
    s_hop_interval = s_rd16_le(&lldata[10]);
    /* Latency at lldata[12..13] — ignored */
    s_supervision = s_rd16_le(&lldata[14]);
    s_chan_map = s_rd40_le(&lldata[16]) & 0x1FFFFFFFFFULL;
    s_hop_increment = lldata[21] & 0x1Fu;
    /* SCA at upper 3 bits of lldata[21] — ignored */

    /* Sanity */
    if (s_hop_interval < 6u || s_hop_increment < 5u || s_hop_increment > 16u) {
        return false;
    }
    return true;
}

/* Callback fired by RadioIF_followAdvOnce when an ADV-channel packet arrives.
 * Filters CONNECT_IND with InitA matching our target. */
static void s_on_adv_packet(const uint8_t *ll_pdu, uint8_t pdu_len, uint8_t channel,
                            int8_t rssi_dbm, void *user) {
    (void)user;
    (void)rssi_dbm;
    if (pdu_len < 2u) {
        return;
    }
    /* ADV channel PDU header: byte0[3:0]=PduType, [6]=TxAdd, [7]=RxAdd; byte1=Length */
    uint8_t pdu_type = ll_pdu[0] & 0x0Fu;
    /* CONNECT_IND PduType = 0x05 (legacy). AdvLen = 34. Total LL = header(2) + 34 = 36. */
    if (pdu_type != 0x05u || pdu_len < 36u) {
        return;
    }
    /* Layout: byte0..1 hdr, byte2..7 InitA (LE), byte8..13 AdvA (LE), byte14..35 LLData */
    const uint8_t *adv_a = &ll_pdu[8];
    if (s_have_target && memcmp(adv_a, s_target_mac, LL_FOLLOWER_MAC_LEN) != 0) {
        return;
    }
    if (!s_parse_connect_ind_lldata(&ll_pdu[14])) {
        return;
    }
    s_connect_ind_pending = true;
    /* Snapshot t_anchor approximation: first listen window opens at
     * end_of_CONNECT_IND + transmitWindowDelay (1.25 ms) + WinOffset.
     * We use RF_getCurrentTime() as a coarse base; per-event drift correction
     * happens in FOLLOWING (Sniffle's afterConnEvent equivalent).
     * 1.25 ms = 5000 RAT ticks. */
    s_next_anchor_rat = RF_getCurrentTime() + 5000u;
    s_event_counter = 0u;
    s_zero_rx_streak = 0u;
    if (s_use_csa2) {
        csa2_computeMapping(s_access_addr, s_chan_map);
    }
    /* Channel: this is captured in ADV state; we compute use_csa2 from
     * ChSel bit in the CONNECT_IND header. ChSel = ll_pdu[0] bit 5. */
    s_use_csa2 = ((ll_pdu[0] & 0x20u) != 0u);
    if (s_use_csa2) {
        csa2_computeMapping(s_access_addr, s_chan_map);
    }
}

static void s_on_data_packet(const uint8_t *ll_pdu, uint8_t pdu_len, uint8_t channel,
                             int8_t rssi_dbm, void *user) {
    (void)user;
    if (pdu_len < 2u) {
        return;
    }
    s_packets_captured++;
    s_last_rx_rat = RF_getCurrentTime();
    s_zero_rx_streak = 0u;

    /* Direction inference for capture-only is non-trivial without seeing
     * the CONNECT_IND TxAdd bits stored. For F8b Track B we emit
     * direction='?'; the host parser can guess from LLID + SN/NESN flips
     * across consecutive packets if needed. */
    if (s_cb.onPacket) {
        s_cb.onPacket(ll_pdu, pdu_len, channel, rssi_dbm, s_event_counter, '?');
    }

    /* LL Control: watch for LL_TERMINATE_IND to end cleanly */
    uint8_t llid = ll_pdu[0] & 0x03u;
    uint8_t length = ll_pdu[1];
    if (llid == 0x03u && length >= 1u && pdu_len >= 3u && ll_pdu[2] == 0x02u) {
        /* LL_TERMINATE_IND — the linked devices are ending the connection.
         * Mark a flag so poll() can emit done after this event finishes. */
        s_zero_rx_streak = 0xFFu; /* sentinel: terminate */
    }
}

/* ── Public API ── */

void LlFollower_init(void) {
    s_state = LL_FOLLOWER_STATE_IDLE;
    s_have_target = false;
    memset(&s_cb, 0, sizeof(s_cb));
    s_packets_captured = 0u;
}

void LlFollower_setCallbacks(const LlFollower_Callbacks *cb) {
    if (cb) {
        s_cb = *cb;
    }
}

bool LlFollower_start(const uint8_t target_mac_le[LL_FOLLOWER_MAC_LEN]) {
    if (s_state != LL_FOLLOWER_STATE_IDLE) {
        return false;
    }
    /* All-zero MAC ⇒ wildcard */
    bool any_nonzero = false;
    for (uint8_t i = 0u; i < LL_FOLLOWER_MAC_LEN; i++) {
        if (target_mac_le[i] != 0u) {
            any_nonzero = true;
            break;
        }
    }
    s_have_target = any_nonzero;
    memcpy(s_target_mac, target_mac_le, LL_FOLLOWER_MAC_LEN);

    s_state = LL_FOLLOWER_STATE_SCAN_ADV;
    s_scan_channel = 37u;
    s_connect_ind_pending = false;
    s_packets_captured = 0u;
    return true;
}

bool LlFollower_stop(void) {
    if (s_state == LL_FOLLOWER_STATE_IDLE) {
        return false;
    }
    s_emit_done(LL_FOLLOWER_DONE_HOST_STOP);
    return true;
}

bool LlFollower_isRunning(void) {
    return s_state != LL_FOLLOWER_STATE_IDLE;
}

void LlFollower_poll(void) {
    switch (s_state) {
    case LL_FOLLOWER_STATE_IDLE:
        return;

    case LL_FOLLOWER_STATE_SCAN_ADV: {
        /* Listen on current ADV channel for ~10 ms. If a CONNECT_IND lands
         * for our target, the callback flips s_connect_ind_pending true and
         * we transition. Otherwise rotate to the next ADV channel. */
        uint32_t end = RF_getCurrentTime() + (10u * 4000u); /* 10 ms in RAT */
        (void)RadioIF_followAdvOnce(s_scan_channel, end, s_on_adv_packet, NULL);

        if (s_connect_ind_pending) {
            s_state = LL_FOLLOWER_STATE_FOLLOWING;
            s_connect_ind_pending = false;
            return;
        }
        s_scan_channel = (s_scan_channel == 39u) ? 37u : (uint8_t)(s_scan_channel + 1u);
        return;
    }

    case LL_FOLLOWER_STATE_FOLLOWING: {
        /* Compute data channel for this event */
        uint8_t chan;
        if (s_use_csa2) {
            chan = csa2_computeChannel((uint32_t)s_event_counter);
        } else {
            /* CSA #1 fallback: lastUnmapped(N) = (N+1)*hop mod 37 */
            chan = (uint8_t)(((uint32_t)(s_event_counter + 1u) * s_hop_increment) % 37u);
        }

        /* Listen for one event window. End time = next anchor + half interval
         * to give us plenty of slack for clock drift on first events. */
        uint32_t window_ticks = (uint32_t)s_hop_interval * 5000u;
        uint32_t end = s_next_anchor_rat + window_ticks;
        uint32_t pre = s_packets_captured;
        (void)RadioIF_followDataOnce(chan, s_access_addr, s_crc_init, end,
                                     s_on_data_packet, NULL);

        /* Termination conditions */
        if (s_zero_rx_streak == 0xFFu) {
            s_emit_done(LL_FOLLOWER_DONE_PEER_TERMINATE);
            return;
        }
        if (s_packets_captured == pre) {
            s_zero_rx_streak++;
            if (s_zero_rx_streak > 5u && s_packets_captured == 0u) {
                /* never synced */
                s_emit_done(LL_FOLLOWER_DONE_SYNC_FAILED);
                return;
            }
            /* supervision: 10 ms units; very generous timeout for capture-only */
            if (RF_getCurrentTime() - s_last_rx_rat >
                (uint32_t)s_supervision * 40000u) {
                s_emit_done(LL_FOLLOWER_DONE_SUPERVISION);
                return;
            }
        }

        /* Advance */
        s_event_counter++;
        s_next_anchor_rat += window_ticks;
        return;
    }
    }
}
```

- [ ] **Step 2: Add to CMakeLists**

Read `firmware/cc1352/CMakeLists.txt` to find the `file(GLOB DRIVERLIB_SOURCES` block. Add `src/ll_follower.c` alphabetically (after `src/ll_manager.c`). Also need to ensure the include path is picked up (the `include/` directory is already on the path).

- [ ] **Step 3: Build firmware**

Run: `cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -25; cd ../../..`
Expected: builds. If `s_zero_rx_streak` is unused in some path, GCC may warn — fine if not -Werror.

- [ ] **Step 4: Commit (header + impl + cmake together)**

```
git add firmware/cc1352/include/ll_follower.h firmware/cc1352/src/ll_follower.c firmware/cc1352/CMakeLists.txt
git commit -m "feat(f8b-trackB): ll_follower module — SCAN_ADV/FOLLOWING state machine + CONNECT_IND parse"
```

---

## Phase 5 — Firmware: command_processor dispatch + ControlTask integration

### Task 11: Wire CMD_FOLLOW_START / STOP into command_processor

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c`

- [ ] **Step 1: Add opcode defines and include**

Edit `firmware/cc1352/src/command_processor.c`:

After the existing `#include "ll_manager.h"` line, add:
```c
#include "ll_follower.h"
```

After the `#define CMD_ATT_DEBUG 0x49u` line, add:
```c
/* F8b Track B — passive connection follower */
#define CMD_FOLLOW_START 0x50u
#define CMD_FOLLOW_STOP 0x51u
```

After the `#define RSP_ATT_DEBUG 0xAAu` line, add:
```c
/* F8b Track B */
#define RSP_LL_PACKET 0xABu
#define RSP_FOLLOW_DONE 0xACu
```

- [ ] **Step 2: Add follower callback definitions (above `static void handle_command`)**

Insert immediately before `static void handle_command(...)`:
```c
/* ── F8b Track B follower callbacks ── */

static void follower_on_packet(const uint8_t *ll_pdu, uint8_t pdu_len, uint8_t channel,
                               int8_t rssi_dbm, uint16_t event_counter, uint8_t direction) {
    /* Wire format: [direction:1][channel:1][rssi:1][event:2LE][ll_pdu:N] */
    uint8_t buf[5 + 257];
    if (pdu_len > sizeof(buf) - 5u) {
        return;
    }
    buf[0] = direction;
    buf[1] = channel;
    buf[2] = (uint8_t)rssi_dbm;
    buf[3] = (uint8_t)(event_counter & 0xFFu);
    buf[4] = (uint8_t)(event_counter >> 8);
    memcpy(&buf[5], ll_pdu, pdu_len);
    OutputIF_sendResponse(RSP_LL_PACKET, 0u, buf, (uint16_t)(5u + pdu_len));
}

static void follower_on_done(const LlFollower_DoneInfo *info) {
    /* Wire format: [reason:1][packets_captured:4LE] */
    uint8_t buf[5];
    buf[0] = info->reason;
    write_u32_le(&buf[1], info->packets_captured);
    OutputIF_sendResponse(RSP_FOLLOW_DONE, 0u, buf, sizeof(buf));
}

static bool follower_callbacks_installed = false;
static void ensure_follower_callbacks(void) {
    if (!follower_callbacks_installed) {
        LlFollower_Callbacks cb = {
            .onPacket = follower_on_packet,
            .onDone = follower_on_done,
        };
        LlFollower_setCallbacks(&cb);
        follower_callbacks_installed = true;
    }
}
```

- [ ] **Step 3: Add dispatch cases**

Find the `switch (cmd)` block in `handle_command()`. After the existing `case CMD_ATT_DEBUG:` block (use grep to locate), insert before the `default:` case:
```c
    case CMD_FOLLOW_START: {
        if (payload_len != LL_FOLLOWER_MAC_LEN) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        ensure_follower_callbacks();
        if (!LlFollower_start(payload)) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        send_ack(seq);
        return;
    }

    case CMD_FOLLOW_STOP: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!LlFollower_stop()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        send_ack(seq);
        return;
    }
```

- [ ] **Step 4: Build**

Run: `cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -20; cd ../../..`
Expected: builds clean.

- [ ] **Step 5: Commit**

```
git add firmware/cc1352/src/command_processor.c
git commit -m "feat(f8b-trackB): dispatch CMD_FOLLOW_START/STOP + RSP_LL_PACKET emission"
```

### Task 12: Drive `LlFollower_poll()` from ControlTask

**Files:**
- Modify: `firmware/cc1352/src/control_task.c`

- [ ] **Step 1: Locate the main poll loop in control_task.c**

Run: `grep -n "ControlTask\|while\|Task_sleep\|poll" firmware/cc1352/src/control_task.c | head -30`
Identify the main task loop where it polls existing modules (e.g., `RadioIF_poll()`, `BleConnMgr_poll()`).

- [ ] **Step 2: Add `LlFollower_poll()` to the loop**

Add `#include "ll_follower.h"` near the existing includes.
Inside the main task loop, after the existing `BleConnMgr_poll()` call (or in a similar position adjacent to other poll() functions), add:
```c
        if (LlFollower_isRunning()) {
            LlFollower_poll();
        }
```

Also add `LlFollower_init();` near the other init calls (e.g., next to `BleConnMgr_init()`).

- [ ] **Step 3: Build**

Run: `cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -20; cd ../../..`
Expected: builds.

- [ ] **Step 4: Flash to board to verify it boots**

Run (with retry per memory rule):
```
make flash 2>&1 | tail -10 || make flash 2>&1 | tail -10
```
Expected: catnip flash succeeds, board re-enumerates.

- [ ] **Step 5: Smoke run init from Python to confirm no boot regression**

Run:
```
cd python && source .venv/bin/activate 2>/dev/null || true
python -c "from feralrf import Radio; r=Radio('/dev/ttyACM2'); r.connect(); info=r.init(); print('OK', info)" 2>&1 | tail -5
cd ..
```
Expected: prints `OK` with DeviceInfo.

- [ ] **Step 6: Commit**

```
git add firmware/cc1352/src/control_task.c
git commit -m "feat(f8b-trackB): drive LlFollower_poll() from ControlTask"
```

---

## Phase 6 — Python: `Radio.follow_connection` + `read_ll_packets`

### Task 13: `follow_connection` / `stop_follow_connection`

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_follow_connection.py`

- [ ] **Step 1: Write failing test for `follow_connection` API**

Append to `python/tests/test_follow_connection.py`:
```python
class TestFollowConnectionAPI:
    """Mocked-serial tests for Radio.follow_connection / read_ll_packets."""

    def _make_radio_with_fake_serial(self, response_frames):
        """Build a Radio with a mocked serial that emits the given frames in order."""
        from unittest.mock import MagicMock
        from feralrf.radio import Radio
        from feralrf.protocol import build_frame, cobs_encode

        r = Radio("/dev/null")
        r._serial = MagicMock()
        r._serial.is_open = True
        # Build COBS-encoded responses concatenated into the read stream
        stream = bytearray()
        for cmd_id, seq, payload in response_frames:
            frame = build_frame(cmd_id, seq, payload)
            stream.extend(frame)

        # MagicMock for serial.read(n): return one byte at a time
        idx = [0]
        def fake_read(n=1):
            if idx[0] >= len(stream):
                return b""
            out = stream[idx[0] : idx[0] + n]
            idx[0] += n
            return bytes(out)
        r._serial.read = fake_read
        r._serial.timeout = 1.0
        return r

    def test_follow_connection_with_mac_sends_correct_command(self):
        from feralrf.radio import Radio
        from feralrf.enums import Response
        r = self._make_radio_with_fake_serial([(Response.ACK, 1, b"")])
        r.follow_connection(target_mac="A8:E6:E8:8A:7D:F8", timeout=1.0)
        # Verify the bytes written to serial included CMD_FOLLOW_START + LE MAC
        write_calls = [c[0][0] for c in r._serial.write.call_args_list]
        assert len(write_calls) == 1
        # The command sent should contain the MAC in LE order (F8 7D 8A E8 E6 A8)
        assert b"\xf8\x7d\x8a\xe8\xe6\xa8" in write_calls[0]

    def test_stop_follow_connection_sends_stop(self):
        from feralrf.enums import Response
        r = self._make_radio_with_fake_serial([(Response.ACK, 1, b"")])
        r.stop_follow_connection(timeout=1.0)
        write_calls = [c[0][0] for c in r._serial.write.call_args_list]
        assert len(write_calls) == 1
```

- [ ] **Step 2: Run test, verify failure**

Run: `cd python && pytest tests/test_follow_connection.py::TestFollowConnectionAPI -x -v 2>&1 | tail -10 && cd ..`
Expected: AttributeError on `follow_connection`.

- [ ] **Step 3: Implement methods in `radio.py`**

Find the `gatt_subscribe` method in `radio.py`. Insert the following methods right after `read_gatt_notifications` (around line 873):
```python
    def follow_connection(
        self,
        target_mac: "str | bytes | None" = None,
        timeout: float = 3.0,
    ) -> None:
        """Start a passive connection follower (F8b Track B).

        Listens on BLE advertising channels for a CONNECT_IND with InitA
        matching target_mac. On capture, switches to data-channel hopping
        per CSA #2 and emits each captured LL data PDU as RSP_LL_PACKET.
        Pure capture — no transmission on the followed link.

        Args:
            target_mac: 6-byte MAC as either an "AA:BB:CC:DD:EE:FF" string,
                raw bytes (already LE), or None for wildcard.
            timeout: Seconds to wait for the firmware ACK.
        """
        if target_mac is None:
            mac_le: "bytes | None" = None
        elif isinstance(target_mac, str):
            mac_le = bytes.fromhex("".join(target_mac.split(":")[::-1]))
        else:
            mac_le = bytes(target_mac)
        self._send_command(
            Command.FOLLOW_START, CommandBuilder.follow_start(mac_le)
        )
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout, expected={Response.ACK, Response.ERROR}
        )
        if cmd_id == Response.ERROR:
            raise CommandError("FOLLOW_START failed", payload[0] if payload else 0)

    def stop_follow_connection(self, timeout: float = 3.0) -> None:
        """Stop the passive connection follower."""
        self._send_command(Command.FOLLOW_STOP, CommandBuilder.follow_stop())
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout, expected={Response.ACK, Response.ERROR}
        )
        if cmd_id == Response.ERROR:
            raise CommandError("FOLLOW_STOP failed", payload[0] if payload else 0)
```

Also add to `STABLE_METHODS` tuple (around line 165): `"follow_connection",`, `"stop_follow_connection",`, `"read_ll_packets",`.

- [ ] **Step 4: Run tests**

Run: `cd python && pytest tests/test_follow_connection.py -x -v 2>&1 | tail -15 && cd ..`
Expected: all tests pass.

- [ ] **Step 5: Commit**

```
git add python/feralrf/radio.py python/tests/test_follow_connection.py
git commit -m "feat(f8b-trackB): Radio.follow_connection + stop_follow_connection"
```

### Task 14: `read_ll_packets` iterator

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_follow_connection.py`

- [ ] **Step 1: Write failing test**

Append to `python/tests/test_follow_connection.py`:
```python
class TestReadLLPackets:
    def test_iterator_yields_parsed_packets(self):
        from feralrf.enums import Response
        from feralrf.radio import LLPacket
        # Wire format: [dir:1][ch:1][rssi:1][event:2LE][ll_pdu:N]
        payload1 = (
            b"M"  # direction
            + bytes([10])  # channel
            + bytes([0xC4])  # rssi -60 as signed byte
            + b"\x05\x00"  # event counter 5
            + b"\x02\x09\x05\x00\x04\x00\x12\xd5\x00\x01\x00"  # LL PDU
        )
        # Make the test simple by reusing the helper from prior class
        helper = TestFollowConnectionAPI()
        r = helper._make_radio_with_fake_serial([(Response.LL_PACKET, 0, payload1)])
        pkts = list(r.read_ll_packets(timeout=0.5))
        assert len(pkts) == 1
        assert isinstance(pkts[0], LLPacket)
        assert pkts[0].channel == 10
        assert pkts[0].rssi_dbm == -60
        assert pkts[0].event_counter == 5
        assert pkts[0].direction == "M"
        assert pkts[0].payload.startswith(b"\x02\x09\x05")

    def test_iterator_ends_on_follow_done(self):
        from feralrf.enums import Response
        helper = TestFollowConnectionAPI()
        # Single FOLLOW_DONE frame should end the iterator quietly
        done_payload = b"\x00" + (0).to_bytes(4, "little")  # reason=HOST_STOP, count=0
        r = helper._make_radio_with_fake_serial([(Response.FOLLOW_DONE, 0, done_payload)])
        pkts = list(r.read_ll_packets(timeout=0.5))
        assert pkts == []

    def test_iterator_ends_on_timeout(self):
        helper = TestFollowConnectionAPI()
        r = helper._make_radio_with_fake_serial([])  # nothing to read → timeout
        pkts = list(r.read_ll_packets(timeout=0.1))
        assert pkts == []
```

- [ ] **Step 2: Run test, verify failure**

Run: `cd python && pytest tests/test_follow_connection.py::TestReadLLPackets -x -v 2>&1 | tail -10 && cd ..`
Expected: `AttributeError: ... has no attribute 'read_ll_packets'`.

- [ ] **Step 3: Implement iterator in `radio.py`**

Insert after `stop_follow_connection`:
```python
    def read_ll_packets(self, timeout: float = 30.0) -> "Iterator[LLPacket]":
        """Yield LLPacket frames as the firmware captures them.

        Iterates over RX frames filtering for RSP_LL_PACKET. A RSP_FOLLOW_DONE
        frame ends the iterator quietly (the follow session terminated). A
        timeout (no frame for `timeout` seconds) also ends the iterator
        quietly so callers can poll-loop.
        """
        while True:
            try:
                cmd_id, _seq, payload = self._read_response(
                    timeout=timeout,
                    expected={Response.LL_PACKET, Response.FOLLOW_DONE},
                )
            except TimeoutError:
                return
            if cmd_id == Response.FOLLOW_DONE:
                return
            if cmd_id != Response.LL_PACKET or len(payload) < 5:
                continue
            direction = chr(payload[0]) if 32 <= payload[0] < 127 else "?"
            channel = payload[1]
            # Convert unsigned byte to signed int8
            rssi_byte = payload[2]
            rssi_dbm = rssi_byte - 256 if rssi_byte > 127 else rssi_byte
            event_counter = int.from_bytes(payload[3:5], "little")
            yield LLPacket(
                direction=direction,
                channel=channel,
                rssi_dbm=rssi_dbm,
                event_counter=event_counter,
                payload=bytes(payload[5:]),
                timestamp=time.monotonic(),
            )
```

- [ ] **Step 4: Run tests**

Run: `cd python && pytest tests/test_follow_connection.py tests/test_ll_parser.py -v 2>&1 | tail -25 && cd ..`
Expected: all pass.

- [ ] **Step 5: Commit**

```
git add python/feralrf/radio.py python/tests/test_follow_connection.py
git commit -m "feat(f8b-trackB): Radio.read_ll_packets iterator + tests"
```

---

## Phase 7 — Hardware smoke test

### Task 15: Smoke harness

**Files:**
- Create: `python/examples/lab/smoke_f8b_follower.py`

- [ ] **Step 1: Create the smoke script**

Create `python/examples/lab/smoke_f8b_follower.py`:
```python
#!/usr/bin/env python3
"""F8b Track B — wire-level smoke for passive connection follower.

Procedure (manual):
  1. Put your phone in Bluetooth-discovery mode.
  2. Power on Sony WH-CH720N (or any BLE peripheral); make sure it is NOT
     already paired with the phone.
  3. Run this script — it will start the follower, then wait for you to
     initiate a pairing on the phone.
  4. The follower captures every LL data PDU on the connection.
  5. After 30 s of follow time (or peer disconnect), packets are dumped
     and a pcap-NG is written to /tmp/f8b_follower.pcapng.

Closure: ≥10 bidirectional LL data PDUs captured, pcap valid.

Usage:  python smoke_f8b_follower.py [--port /dev/ttyACM2] [--target-mac AA:BB:CC:DD:EE:FF] [--duration 30]
"""
import argparse
import sys
import time

from feralrf import Radio
from feralrf._ll_parser import export_pcap, parse_ll_pdu


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="/dev/ttyACM2")
    p.add_argument("--target-mac", default="A8:E6:E8:8A:7D:F8",
                   help="Sony WH-CH720N MAC by default; use 'wildcard' to capture any")
    p.add_argument("--duration", type=float, default=30.0)
    p.add_argument("--pcap", default="/tmp/f8b_follower.pcapng")
    args = p.parse_args()

    r = Radio(args.port)
    r.connect()
    time.sleep(0.3)
    r.init()

    target = None if args.target_mac.lower() == "wildcard" else args.target_mac
    print(f"[STEP] follow_connection target={target or 'wildcard'}")
    r.follow_connection(target_mac=target, timeout=5.0)

    print(f"[STEP] capturing for {args.duration:.0f} s — initiate pairing on phone now")
    pkts = []
    t0 = time.time()
    while time.time() - t0 < args.duration:
        for p in r.read_ll_packets(timeout=1.0):
            pkts.append(p)
            kind = parse_ll_pdu(p.payload)
            kname = kind.kind.name if kind else "?"
            print(f"  [{time.time()-t0:5.1f}s] ch{p.channel:>2} ev{p.event_counter:>4} "
                  f"{kname:>11} rssi={p.rssi_dbm:+4d} len={len(p.payload):>3}")

    try:
        r.stop_follow_connection(timeout=2.0)
    except Exception as e:
        print(f"  stop returned {type(e).__name__}: {e} (ok if peer terminated)")
    r.disconnect()

    print(f"\n[STEP] export pcap → {args.pcap}")
    export_pcap(pkts, args.pcap)

    print()
    if len(pkts) >= 10:
        print(f"[ OK ] F8b Track B smoke PASS — captured {len(pkts)} packets")
        return 0
    print(f"[FAIL] only {len(pkts)} packets captured (need ≥10)")
    return 1


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Run the smoke against real hardware**

User must initiate phone↔Sony pairing during the 30 s window.

Run:
```
cd python && source .venv/bin/activate 2>/dev/null || true
python examples/lab/smoke_f8b_follower.py --port /dev/ttyACM2 --target-mac A8:E6:E8:8A:7D:F8 2>&1 | tail -40
cd ..
```
Expected: `[ OK ] F8b Track B smoke PASS — captured N packets` with N ≥ 10. If sync fails (`captured 0 packets`), retry — capturing CONNECT_IND has timing noise. Three retries before declaring failure.

If sync repeatedly fails, the most likely cause is that ADV scan window (10 ms per channel × 3 channels = 30 ms cycle) misses the brief CONNECT_IND. Mitigation: reduce per-channel dwell to 5 ms in `ll_follower.c` `s_state == SCAN_ADV` case (rebuild + retry).

- [ ] **Step 3: Verify pcap opens cleanly**

Open `/tmp/f8b_follower.pcapng` in Wireshark or `tshark -r /tmp/f8b_follower.pcapng | head -20`.
Expected: at least one ATT frame visible, packet count matches the script's reported count.

- [ ] **Step 4: Re-run F8b Track A smoke to confirm no regression**

Run:
```
cd python && python examples/lab/smoke_f8b_notifications.py --port /dev/ttyACM2 --target-mac A8:E6:E8:8A:7D:F8 --duration 20 2>&1 | tail -15 && cd ..
```
Expected: `[ OK ] F8b Track A smoke PASS — captured N notifications` with N ≥ 1.

- [ ] **Step 5: Commit smoke harness**

```
git add python/examples/lab/smoke_f8b_follower.py
git commit -m "test(f8b-trackB): hardware smoke against Sony WH-CH720N + phone pair"
```

---

## Phase 8 — Pre-commit + tag

### Task 16: Pre-commit clean across modified files

**Files:** all touched in this branch

- [ ] **Step 1: Get the file list**

Run: `git diff --name-only feature/ti-rtos-migration..HEAD`
Note the list.

- [ ] **Step 2: Run pre-commit on those files (NEVER --all-files per memory rule)**

Run:
```
pre-commit run --files $(git diff --name-only feature/ti-rtos-migration..HEAD | tr '\n' ' ')
```
Expected: all hooks pass. Fix and re-stage anything that doesn't.

- [ ] **Step 3: If pre-commit modified any files, commit fixes**

```
git status --short
git add <modified files>
git commit -m "style(f8b-trackB): pre-commit fixes"
```

### Task 17: Tag v2.0-f8b-trackB (gated on user OK)

**Files:** none

- [ ] **Step 1: Verify acceptance gates met**

Confirm:
- ✅ Smoke ≥10 packets captured
- ✅ pcap opens in Wireshark
- ✅ Unit tests pass: run `cd python && pytest tests/test_ll_parser.py tests/test_follow_connection.py -v 2>&1 | tail -10 && cd ..`
- ✅ F8b Track A smoke not regressed
- ✅ Firmware builds clean
- ✅ Pre-commit clean

- [ ] **Step 2: Ask user before tagging — DO NOT tag autonomously**

Per FeralRF convention (memory: project_f9_done, project_f22_done), tags land **only after** explicit user approval. Print a summary and wait for confirmation:
```
[Phase 8 / Task 17] Track B work complete:
  - {N} commits
  - Smoke {N_PKTS} packets captured
  - pcap valid
  - {X} unit tests passing
Ready to tag v2.0-f8b-trackB on commit {SHA}? (waiting for your OK)
```

- [ ] **Step 3: After user OK, tag and FF into ti-rtos-migration**

```
git tag -a v2.0-f8b-trackB -m "F8b Track B — Sniffle-style passive connection follower"
git checkout feature/ti-rtos-migration
git merge --ff-only feature/f8b-track-b
```

- [ ] **Step 4: Save closure memory**

Write `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f8b_track_b_done.md` with: date, commit SHA, packet count, pcap location, any deferred follow-ups.

Update `MEMORY.md` with the new entry.

---

## Self-review notes

**Spec coverage:**
- (1) Notifications ⇒ already shipped Track A — out of scope of this plan ✓
- (2b) Connection follower ⇒ Tasks 7-15 ✓
- (3) Pairing ⇒ deferred to Track C — explicitly out of scope ✓
- AttClient stale-state bug ⇒ NOT addressed in Track B; per memory `project_gatt_attclient_bug.md` not reproducible in HEAD; if it resurfaces during smoke (Task 15), document and defer to Track C plan ✓

**Vendoring decision (deviation from spec):**
The spec proposed vendoring `csa2.c`, `adv_header_cache.c`, `AuxAdvScheduler.c`, `DelayHopTrigger.c`, `DelayStopTrigger.c` from Sniffle (~5 files, ~500 LOC).

This plan deviates: FeralRF already has `csa2.c` (verified identical, both GPL-3.0). The other Sniffle helpers serve advanced needs (extended advertising, anchor postponement during active scan) that Track B doesn't require for capture-only. The plan reuses `Ble5_0_cmdBle5GenericRx` (already wired in `radio_if.c`) wrapped in two new helpers, and ports only the **state-machine logic** from Sniffle's `RadioTask.c`. Net firmware delta: ~250 LOC vs spec's ~550 LOC. Provenance noted in `ll_follower.c` header comment.

If F8c needs extended advertising support, vendoring `AuxAdvScheduler.c` then will be cheap incremental work.

**Direction inference (deferred):**
`s_on_data_packet` emits `direction='?'`. Reliable direction inference requires tracking per-PDU SN/NESN flips against a previous PDU on the same event — non-trivial, deferred to F8c. Host parser can post-process if needed.

**RF mode coexistence:**
While `LlFollower_isRunning()`, no other BLE operation may run (they share `s_rf_handle` + `Ble5_0_cmdBle5GenericRx`). `command_processor.c` does not currently guard this — if a user starts the follower then issues `ble_connect`, both will fight over the RF core. **Acceptable for F8b Track B**: documented behavior; user must `stop_follow_connection()` first. Hard guard can be added in F8c if it bites in practice.

**No placeholders verified:** every code step has complete code; every `Expected:` line names a concrete observable result.
