# F8 — Validate GATT end-to-end Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose the ATT/GATT client built in F7 through the public Python `Radio` API, refactor the local `test_connect.py` script into a lab demo using that API, and validate GATT service discovery + characteristic read/write end-to-end against a real BLE peripheral.

**Architecture:** Firmware already implements `CMD_CONNECT/DISCONNECT/CONN_STATUS/GATT_DISCOVER/GATT_READ/GATT_WRITE` and the matching `RSP_CONN_RESULT/CONN_STATUS_R/GATT_SERVICE/GATT_CHAR/GATT_READ_R/GATT_DONE` responses. This plan is **Python-only**: it wires those wire-format bytes into typed `Radio` methods, adds unit tests against mocked serial, adds a hardware-gated integration test, and ships a replacement demo. No firmware changes unless a blocker is found during validation.

**Tech Stack:** Python 3.9+, `pyserial`, `cobs`, `pytest`. Firmware side is read-only for F8.

**Reference:** Spec §5 "F8 — Validar GATT end-to-end" at `docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md`.

**Depends on:** F7 complete (branch `feature/ti-rtos-migration` at commit `41b81fe`).

**Resolves pending decisions:** D1 (peripheral real), D2 (ubicación de `test_connect.py`).

---

## Pending decisions to resolve before Task 1

Two decisions from the spec (§9) must be resolved before coding starts. The answers drive the paths and test targets in this plan.

- **D1 — Peripheral real for validation.** Pick one and document:
  - Smartphone + BLE peripheral simulator app (fast, no extra HW).
  - ESP32 or nRF52840 flashed with a GATT server firmware.
  - Raspberry Pi running `bleno`.
  The plan's "Hardware required" rows assume **a smartphone running a BLE peripheral simulator app**. Substitute below if a different target is chosen.
- **D2 — Final location of the local `test_connect.py`.** The plan assumes it moves to `python/examples/lab/demo_ble_connect_gatt.py` (consistent with `demo_ble_analyzer.py`, `demo_ble_clone.py`, `demo_emulate_soundcore.py` that already live there). Adjust paths if a different location is chosen.

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `python/feralrf/enums.py` | Modify | Add `Command.CONNECT/DISCONNECT/CONN_STATUS/GATT_DISCOVER/GATT_READ/GATT_WRITE` and `Response.CONN_RESULT/CONN_STATUS/GATT_SERVICE/GATT_CHAR/GATT_READ_VALUE/GATT_DONE` |
| `python/feralrf/commands.py` | Modify | Add `CommandBuilder` static methods: `ble_connect`, `ble_disconnect`, `conn_status`, `gatt_discover`, `gatt_read`, `gatt_write` |
| `python/feralrf/radio.py` | Modify | Add dataclasses `ConnectionResult`, `ConnectionStatus`, `GattService`, `GattCharacteristic`, `GattDiscoveryResult`; add methods `ble_connect`, `ble_disconnect`, `conn_status`, `gatt_discover`, `gatt_read`, `gatt_write`; move `gatt_discovery` out of `PENDING_FEATURES`; extend `STABLE_METHODS` |
| `python/feralrf/__init__.py` | Modify | Re-export new dataclasses |
| `python/examples/lab/demo_ble_connect_gatt.py` | Create | Scan → pick connectable → `ble_connect` → `gatt_discover` → optional `gatt_read` → `ble_disconnect`, using the `Radio` API |
| `python/tests/test_gatt_api.py` | Create | Unit tests for command payload builders + response parsers (mocked serial) |
| `python/tests/test_gatt_integration.py` | Create | Hardware-gated integration test (pytest marker `hardware_ble`) |
| `python/pyproject.toml` | Modify | Register `hardware_ble` pytest marker |
| `test_connect.py` (repo root) | Delete | Superseded by `demo_ble_connect_gatt.py` |
| `docs/PYTHON_API.md` | Modify | Add GATT section under public API |

---

### Task 1: Create feature branch and verify clean baseline

**Files:** none modified.

- [ ] **Step 1: Confirm starting commit and clean tree**

Run:
```bash
git status
git log --oneline -1
```

Expected: branch `feature/ti-rtos-migration`, HEAD at `1cfe504` (spec commit), only `test_connect.py` untracked.

- [ ] **Step 2: Create feature branch**

Run:
```bash
git checkout -b feature/f8-gatt-validation
git status
```

Expected: switched to `feature/f8-gatt-validation`, `test_connect.py` still untracked (carried over).

- [ ] **Step 3: Document D1 and D2 resolutions in the spec**

Modify `docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md` §9. Replace the rows for D1 and D2 with the actual decision (e.g. "Smartphone with BLE Peripheral Simulator app" and "python/examples/lab/demo_ble_connect_gatt.py"). Keep the responsible/resolved-in columns as is.

- [ ] **Step 4: Commit the decision update**

Run:
```bash
git add docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md
git commit -m "docs(f8): resolve pending decisions D1 and D2"
```

Expected: pre-commit hooks pass, commit created.

---

### Task 2: Add Command enum values for BLE connection and GATT

**Files:**
- Modify: `python/feralrf/enums.py`
- Test: `python/tests/test_gatt_api.py` (new)

- [ ] **Step 1: Create the failing test**

Create `python/tests/test_gatt_api.py` with:

```python
"""FeralRF — GATT API unit tests (no hardware)."""

from feralrf.enums import Command, Response


def test_command_enum_has_ble_connection_and_gatt_ids():
    # BLE connection
    assert Command.CONNECT == 0x40
    assert Command.DISCONNECT == 0x41
    assert Command.CONN_STATUS == 0x42
    # GATT
    assert Command.GATT_DISCOVER == 0x43
    assert Command.GATT_READ == 0x45
    assert Command.GATT_WRITE == 0x46


def test_response_enum_has_connection_and_gatt_ids():
    # Connection
    assert Response.CONN_RESULT == 0xA0
    assert Response.CONN_STATUS == 0xA1
    # GATT
    assert Response.GATT_SERVICE == 0xA2
    assert Response.GATT_CHAR == 0xA3
    assert Response.GATT_READ_VALUE == 0xA4
    assert Response.GATT_DONE == 0xA5
```

- [ ] **Step 2: Run the test and confirm it fails**

Run:
```bash
cd python && source .venv/bin/activate && pytest tests/test_gatt_api.py -v
```

Expected: FAIL with `AttributeError: CONNECT` on `Command` enum.

- [ ] **Step 3: Extend the `Command` enum**

In `python/feralrf/enums.py`, inside `class Command(IntEnum)`, after the `JAM_STOP = 0x33` line, add:

```python
    # BLE Connection
    CONNECT = 0x40
    DISCONNECT = 0x41
    CONN_STATUS = 0x42

    # GATT
    GATT_DISCOVER = 0x43
    GATT_READ = 0x45
    GATT_WRITE = 0x46
```

- [ ] **Step 4: Extend the `Response` enum**

In the same file, inside `class Response(IntEnum)`, after `INFO = 0x94`, add:

```python
    # BLE Connection
    CONN_RESULT = 0xA0
    CONN_STATUS = 0xA1

    # GATT
    GATT_SERVICE = 0xA2
    GATT_CHAR = 0xA3
    GATT_READ_VALUE = 0xA4
    GATT_DONE = 0xA5
```

- [ ] **Step 5: Run the test again and confirm it passes**

Run:
```bash
pytest tests/test_gatt_api.py -v
```

Expected: both tests PASS.

- [ ] **Step 6: Commit**

Run:
```bash
git add python/feralrf/enums.py python/tests/test_gatt_api.py
git commit -m "feat(f8): add BLE CONNECT/GATT command and response enum IDs"
```

---

### Task 3: Add CommandBuilder payload builders for BLE/GATT commands

**Files:**
- Modify: `python/feralrf/commands.py`
- Test: `python/tests/test_gatt_api.py`

- [ ] **Step 1: Write the failing test**

Append to `python/tests/test_gatt_api.py`:

```python
from feralrf.commands import CommandBuilder


def test_ble_connect_payload_is_addr_le_plus_type():
    # Address must be passed as 6 little-endian bytes (wire order).
    # Public-type example.
    addr_le = b"\x01\xEE\xDD\xCC\xBB\xAA"
    assert CommandBuilder.ble_connect(addr_le, addr_type=0) == addr_le + b"\x00"
    # Random-type example.
    assert CommandBuilder.ble_connect(addr_le, addr_type=1) == addr_le + b"\x01"


def test_ble_connect_rejects_wrong_length():
    import pytest as _pt
    with _pt.raises(ValueError):
        CommandBuilder.ble_connect(b"\x01\x02\x03", addr_type=0)


def test_ble_disconnect_and_conn_status_are_empty():
    assert CommandBuilder.ble_disconnect() == b""
    assert CommandBuilder.conn_status() == b""


def test_gatt_discover_is_empty():
    assert CommandBuilder.gatt_discover() == b""


def test_gatt_read_payload_is_u16_le_handle():
    assert CommandBuilder.gatt_read(0x002A) == b"\x2A\x00"


def test_gatt_write_payload_is_handle_plus_data():
    assert CommandBuilder.gatt_write(0x002A, b"\xDE\xAD\xBE\xEF") == b"\x2A\x00\xDE\xAD\xBE\xEF"


def test_gatt_write_allows_empty_data():
    assert CommandBuilder.gatt_write(0x0010, b"") == b"\x10\x00"
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run:
```bash
pytest tests/test_gatt_api.py -v
```

Expected: FAIL with `AttributeError: ble_connect` on `CommandBuilder`.

- [ ] **Step 3: Implement the builders**

Append to `python/feralrf/commands.py` (inside `class CommandBuilder`):

```python
    @staticmethod
    def ble_connect(addr_le: bytes, addr_type: int) -> bytes:
        """Payload for CMD_CONNECT: 6-byte LE address + 1-byte address type.

        Args:
            addr_le: Peer address in little-endian wire order (reversed of AA:BB:CC:DD:EE:FF).
            addr_type: 0 for public, 1 for random.
        """
        if len(addr_le) != 6:
            raise ValueError("addr_le must be exactly 6 bytes")
        return bytes(addr_le) + bytes([addr_type & 0xFF])

    @staticmethod
    def ble_disconnect() -> bytes:
        """No payload for CMD_DISCONNECT."""
        return b""

    @staticmethod
    def conn_status() -> bytes:
        """No payload for CMD_CONN_STATUS."""
        return b""

    @staticmethod
    def gatt_discover() -> bytes:
        """No payload for CMD_GATT_DISCOVER."""
        return b""

    @staticmethod
    def gatt_read(handle: int) -> bytes:
        """Payload for CMD_GATT_READ: 2-byte LE attribute handle."""
        return struct.pack("<H", handle & 0xFFFF)

    @staticmethod
    def gatt_write(handle: int, data: bytes) -> bytes:
        """Payload for CMD_GATT_WRITE: 2-byte LE handle + value bytes."""
        return struct.pack("<H", handle & 0xFFFF) + bytes(data)
```

- [ ] **Step 4: Run the tests and confirm they pass**

Run:
```bash
pytest tests/test_gatt_api.py -v
```

Expected: all 9 tests PASS.

- [ ] **Step 5: Commit**

Run:
```bash
git add python/feralrf/commands.py python/tests/test_gatt_api.py
git commit -m "feat(f8): add CommandBuilder methods for BLE connection and GATT"
```

---

### Task 4: Add dataclasses for connection and GATT results

**Files:**
- Modify: `python/feralrf/radio.py`
- Modify: `python/feralrf/__init__.py`
- Test: `python/tests/test_gatt_api.py`

- [ ] **Step 1: Write the failing test**

Append to `python/tests/test_gatt_api.py`:

```python
from feralrf.radio import (
    ConnectionResult,
    ConnectionStatus,
    GattCharacteristic,
    GattDiscoveryResult,
    GattService,
)


def test_connection_result_dataclass():
    r = ConnectionResult(result=0)
    assert r.result == 0
    assert r.is_ok


def test_connection_result_is_ok_false_when_nonzero():
    assert ConnectionResult(result=1).is_ok is False


def test_connection_status_minimum_fields():
    s = ConnectionStatus(connected=True, interval=40, events=3, last_status=0x1400)
    assert s.connected is True
    assert s.interval == 40


def test_gatt_service_fields():
    svc = GattService(start_handle=0x0001, end_handle=0x0005, uuid=b"\x00\x18")
    assert svc.start_handle == 0x0001
    assert svc.end_handle == 0x0005
    assert svc.uuid == b"\x00\x18"


def test_gatt_characteristic_fields():
    ch = GattCharacteristic(handle=0x0002, properties=0x02, value_handle=0x0003, uuid=b"\x00\x2A")
    assert ch.handle == 0x0002
    assert ch.properties == 0x02
    assert ch.value_handle == 0x0003


def test_gatt_discovery_result_is_empty_by_default():
    res = GattDiscoveryResult(services=[], characteristics=[], status=0)
    assert res.services == []
    assert res.characteristics == []
    assert res.status == 0
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run:
```bash
pytest tests/test_gatt_api.py -v
```

Expected: FAIL with `ImportError: cannot import name 'ConnectionResult'`.

- [ ] **Step 3: Add dataclasses to `radio.py`**

In `python/feralrf/radio.py`, after the existing `DeviceStats` dataclass (around line 55), add:

```python
@dataclass
class ConnectionResult:
    """Result of a BLE CMD_CONNECT attempt.

    Result codes mirror the firmware:
        0: OK
        1: TIMEOUT
        2: NO_SYNC
        3: RF_ERR
    """

    result: int

    @property
    def is_ok(self) -> bool:
        return self.result == 0


@dataclass
class ConnectionStatus:
    """Snapshot of the current BLE central connection.

    Fields after `last_status` are only populated when the firmware
    includes the extended debug block (F7 telemetry; may be removed
    after F8 validation).
    """

    connected: bool
    interval: int
    events: int
    last_status: int
    tx_done: Optional[int] = None
    att_state: Optional[int] = None
    total_rx: Optional[int] = None


@dataclass
class GattService:
    """A GATT primary service discovered on the peer.

    uuid is the raw LE bytes as reported by the peer: 2 bytes for a
    16-bit UUID, 16 bytes for a full UUID.
    """

    start_handle: int
    end_handle: int
    uuid: bytes


@dataclass
class GattCharacteristic:
    """A GATT characteristic discovered on the peer."""

    handle: int
    properties: int
    value_handle: int
    uuid: bytes


@dataclass
class GattDiscoveryResult:
    """Aggregated output of a full gatt_discover() call."""

    services: list
    characteristics: list
    status: int
```

- [ ] **Step 4: Re-export the new dataclasses from the package**

In `python/feralrf/__init__.py`, extend the `from feralrf.radio import ...` line (or add one) so that these names are part of the top-level package. For example:

```python
from feralrf.radio import (
    ConnectionResult,
    ConnectionStatus,
    DeviceInfo,
    DeviceStats,
    GattCharacteristic,
    GattDiscoveryResult,
    GattService,
    Packet,
    Radio,
)
```

(Preserve any other imports already in the file — read it first and merge.)

- [ ] **Step 5: Run the tests and confirm they pass**

Run:
```bash
pytest tests/test_gatt_api.py -v
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

Run:
```bash
git add python/feralrf/radio.py python/feralrf/__init__.py python/tests/test_gatt_api.py
git commit -m "feat(f8): add connection and GATT result dataclasses"
```

---

### Task 5: Implement `Radio.ble_connect()` and `Radio.ble_disconnect()`

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_gatt_api.py`

**Naming note:** the existing `Radio.connect()` and `Radio.disconnect()` manage the serial port and MUST NOT be shadowed. BLE-layer methods are named `ble_connect` / `ble_disconnect` to avoid the collision.

- [ ] **Step 1: Write the failing unit test using a fake serial**

Append to `python/tests/test_gatt_api.py`:

```python
import struct
from typing import Optional, List, Tuple

from feralrf.enums import Command, Response
from feralrf.protocol import build_frame, parse_frame, cobs_decode
from feralrf.radio import Radio


class FakeSerial:
    """Minimal serial stand-in for Radio unit tests.

    Captures frames written by the Radio and plays back pre-canned
    response frames on read. Each call to `queue_response()` adds one
    COBS-delimited frame to the read buffer.
    """

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

    def written_frames(self) -> List[Tuple[int, int, bytes]]:
        """Decode all frames written by the Radio."""
        frames: List[Tuple[int, int, bytes]] = []
        buf = bytearray()
        for b in self.written:
            if b == 0x00:
                if buf:
                    decoded = cobs_decode(bytes(buf))
                    frames.append(parse_frame(decoded))
                buf = bytearray()
            else:
                buf.append(b)
        return frames


def _radio_with_fake_serial() -> Tuple[Radio, FakeSerial]:
    radio = Radio(port="/dev/null")
    fake = FakeSerial()
    radio._serial = fake  # type: ignore[assignment]
    return radio, fake


def test_ble_connect_sends_correct_frame_and_parses_result():
    radio, fake = _radio_with_fake_serial()
    addr_le = b"\x01\xEE\xDD\xCC\xBB\xAA"

    # Pre-queue the expected RSP_CONN_RESULT(0) with the seq the radio will use.
    fake.queue_response(Response.CONN_RESULT, seq=0, payload=b"\x00")

    result = radio.ble_connect(addr_le, addr_type=1, timeout=1.0)

    assert isinstance(result, ConnectionResult)
    assert result.is_ok
    frames = fake.written_frames()
    assert len(frames) == 1
    cmd_id, _seq, payload = frames[0]
    assert cmd_id == Command.CONNECT
    assert payload == addr_le + b"\x01"


def test_ble_connect_returns_nonzero_on_timeout_code():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.CONN_RESULT, seq=0, payload=b"\x01")  # TIMEOUT
    result = radio.ble_connect(b"\x01\xEE\xDD\xCC\xBB\xAA", addr_type=1, timeout=1.0)
    assert result.result == 1
    assert not result.is_ok


def test_ble_disconnect_sends_cmd_disconnect_and_accepts_ack():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ACK, seq=0)
    radio.ble_disconnect(timeout=1.0)
    frames = fake.written_frames()
    assert len(frames) == 1
    assert frames[0][0] == Command.DISCONNECT
    assert frames[0][2] == b""
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run:
```bash
pytest tests/test_gatt_api.py::test_ble_connect_sends_correct_frame_and_parses_result -v
```

Expected: FAIL with `AttributeError: 'Radio' object has no attribute 'ble_connect'`.

- [ ] **Step 3: Implement `ble_connect` and `ble_disconnect`**

In `python/feralrf/radio.py`, inside class `Radio`, after `set_ble_scan_mode()` (around line 473), add:

```python
    def ble_connect(
        self, addr_le: bytes, addr_type: int, timeout: float = 8.0
    ) -> ConnectionResult:
        """Issue CMD_CONNECT as BLE central; blocks until RSP_CONN_RESULT.

        Args:
            addr_le: 6-byte peer address in little-endian wire order
                (reversed of AA:BB:CC:DD:EE:FF).
            addr_type: 0 for public, 1 for random.
            timeout: Seconds to wait for RSP_CONN_RESULT (firmware initiator
                may block up to 5 s per connect attempt).
        """
        self._send_command(
            Command.CONNECT,
            CommandBuilder.ble_connect(addr_le, addr_type),
        )
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.CONN_RESULT, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("CONNECT failed", payload[0] if payload else 0)
        if cmd_id != Response.CONN_RESULT:
            raise ProtocolError(f"Unexpected response to CONNECT: 0x{cmd_id:02X}")
        if not payload:
            raise ProtocolError("CONN_RESULT payload empty")
        return ConnectionResult(result=payload[0])

    def ble_disconnect(self, timeout: float = 2.0) -> None:
        """Issue CMD_DISCONNECT; firmware returns to idle."""
        self._send_command(Command.DISCONNECT, CommandBuilder.ble_disconnect())
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("DISCONNECT failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to DISCONNECT: 0x{cmd_id:02X}")
```

- [ ] **Step 4: Run the tests and confirm they pass**

Run:
```bash
pytest tests/test_gatt_api.py -v
```

Expected: all tests PASS.

- [ ] **Step 5: Commit**

Run:
```bash
git add python/feralrf/radio.py python/tests/test_gatt_api.py
git commit -m "feat(f8): add Radio.ble_connect() and Radio.ble_disconnect()"
```

---

### Task 6: Implement `Radio.conn_status()`

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_gatt_api.py`

The firmware packs CONN_STATUS as: `[connected:1][interval:2 LE][reserved:2][events:2 LE][last_status:2 LE]` (9 bytes minimum) and optionally appends `[tx_done:2 LE][att_state:1][total_rx:2 LE]` (14 bytes total) when the F7 debug block is enabled.

- [ ] **Step 1: Write the failing test**

Append to `python/tests/test_gatt_api.py`:

```python
def test_conn_status_parses_short_payload():
    radio, fake = _radio_with_fake_serial()
    payload = struct.pack("<BHHH", 1, 40, 0, 5) + b"\x00\x14"  # last_status=0x1400
    # The firmware puts 2 reserved bytes between interval and events: rebuild to 9 bytes exactly.
    payload = (
        bytes([1])                       # connected
        + struct.pack("<H", 40)          # interval
        + b"\x00\x00"                    # reserved
        + struct.pack("<H", 5)           # events
        + struct.pack("<H", 0x1400)      # last_status
    )
    assert len(payload) == 9
    fake.queue_response(Response.CONN_STATUS, seq=0, payload=payload)

    status = radio.conn_status(timeout=1.0)
    assert status.connected is True
    assert status.interval == 40
    assert status.events == 5
    assert status.last_status == 0x1400
    assert status.tx_done is None
    assert status.att_state is None


def test_conn_status_parses_extended_payload():
    radio, fake = _radio_with_fake_serial()
    payload = (
        bytes([1])
        + struct.pack("<H", 40)
        + b"\x00\x00"
        + struct.pack("<H", 5)
        + struct.pack("<H", 0x1400)
        + struct.pack("<H", 7)     # tx_done
        + bytes([3])               # att_state
        + struct.pack("<H", 12)    # total_rx
    )
    assert len(payload) == 14
    fake.queue_response(Response.CONN_STATUS, seq=0, payload=payload)

    status = radio.conn_status(timeout=1.0)
    assert status.tx_done == 7
    assert status.att_state == 3
    assert status.total_rx == 12
```

- [ ] **Step 2: Run the tests and confirm they fail**

Run:
```bash
pytest tests/test_gatt_api.py::test_conn_status_parses_short_payload -v
```

Expected: FAIL with `AttributeError: conn_status`.

- [ ] **Step 3: Implement `conn_status()`**

In `python/feralrf/radio.py`, inside class `Radio`, after `ble_disconnect`, add:

```python
    def conn_status(self, timeout: float = 2.0) -> ConnectionStatus:
        """Issue CMD_CONN_STATUS and return the parsed ConnectionStatus.

        The firmware always returns at least 9 bytes. The extra F7 debug
        fields (tx_done, att_state, total_rx) are optional and populated
        only when the firmware includes them.
        """
        self._send_command(Command.CONN_STATUS, CommandBuilder.conn_status())
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.CONN_STATUS, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("CONN_STATUS failed", payload[0] if payload else 0)
        if cmd_id != Response.CONN_STATUS:
            raise ProtocolError(f"Unexpected response to CONN_STATUS: 0x{cmd_id:02X}")
        if len(payload) < 9:
            raise ProtocolError(f"CONN_STATUS payload too short: {len(payload)}")

        connected = bool(payload[0])
        interval = int.from_bytes(payload[1:3], "little")
        # payload[3:5] reserved
        events = int.from_bytes(payload[5:7], "little")
        last_status = int.from_bytes(payload[7:9], "little")

        tx_done = att_state = total_rx = None
        if len(payload) >= 14:
            tx_done = int.from_bytes(payload[9:11], "little")
            att_state = payload[11]
            total_rx = int.from_bytes(payload[12:14], "little")

        return ConnectionStatus(
            connected=connected,
            interval=interval,
            events=events,
            last_status=last_status,
            tx_done=tx_done,
            att_state=att_state,
            total_rx=total_rx,
        )
```

- [ ] **Step 4: Run the tests and confirm they pass**

Run:
```bash
pytest tests/test_gatt_api.py -v
```

Expected: all tests PASS.

- [ ] **Step 5: Commit**

Run:
```bash
git add python/feralrf/radio.py python/tests/test_gatt_api.py
git commit -m "feat(f8): add Radio.conn_status()"
```

---

### Task 7: Implement `Radio.gatt_discover()` (service + characteristic stream)

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_gatt_api.py`

Firmware streams `RSP_GATT_SERVICE` and `RSP_GATT_CHAR` interleaved, terminated by `RSP_GATT_DONE`. The host MUST allow interleaved `RSP_CONN_STATUS` (e.g. if a parallel status poll happens) and ignore them.

**Payload formats (from firmware `att_client.c`):**
- `RSP_GATT_SERVICE`: `[start_handle:2 LE][end_handle:2 LE][uuid: 2 or 16 bytes LE]`
- `RSP_GATT_CHAR`: `[handle:2 LE][properties:1][value_handle:2 LE][uuid: 2 or 16 bytes LE]`
- `RSP_GATT_DONE`: `[status:1]` (0 = OK)

- [ ] **Step 1: Write the failing test**

Append to `python/tests/test_gatt_api.py`:

```python
def test_gatt_discover_collects_services_and_chars_until_done():
    radio, fake = _radio_with_fake_serial()

    # Firmware sends: ACK (optional), service, char, char, service, char, done.
    # The current firmware sends ACK first for CMD_GATT_DISCOVER — the host
    # must consume it and keep reading the stream.
    fake.queue_response(Response.ACK, seq=0)

    # Service 1: 0x0001-0x0005, UUID 0x1800 (Generic Access)
    fake.queue_response(
        Response.GATT_SERVICE,
        seq=0xFF,
        payload=struct.pack("<HH", 0x0001, 0x0005) + b"\x00\x18",
    )
    # Char: handle 0x0002, props=0x02 (read), val=0x0003, UUID 0x2A00 (Device Name)
    fake.queue_response(
        Response.GATT_CHAR,
        seq=0xFF,
        payload=struct.pack("<HBH", 0x0002, 0x02, 0x0003) + b"\x00\x2A",
    )
    # Service 2: 0x0010-0x0014, UUID 0x180F (Battery Service)
    fake.queue_response(
        Response.GATT_SERVICE,
        seq=0xFF,
        payload=struct.pack("<HH", 0x0010, 0x0014) + b"\x0F\x18",
    )
    # Char: handle 0x0011, props=0x10 (notify), val=0x0012, UUID 0x2A19 (Battery Level)
    fake.queue_response(
        Response.GATT_CHAR,
        seq=0xFF,
        payload=struct.pack("<HBH", 0x0011, 0x10, 0x0012) + b"\x19\x2A",
    )
    fake.queue_response(Response.GATT_DONE, seq=0xFF, payload=b"\x00")

    result = radio.gatt_discover(timeout=5.0)

    assert isinstance(result, GattDiscoveryResult)
    assert len(result.services) == 2
    assert len(result.characteristics) == 2
    assert result.services[0].start_handle == 0x0001
    assert result.services[0].uuid == b"\x00\x18"
    assert result.characteristics[0].properties == 0x02
    assert result.status == 0


def test_gatt_discover_raises_on_error_before_done():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ERROR, seq=0, payload=b"\x05")
    import pytest as _pt
    with _pt.raises(CommandError):
        radio.gatt_discover(timeout=1.0)
```

Add the missing import near the top of `test_gatt_api.py`:

```python
from feralrf.exceptions import CommandError
```

- [ ] **Step 2: Run and confirm it fails**

Run:
```bash
pytest tests/test_gatt_api.py::test_gatt_discover_collects_services_and_chars_until_done -v
```

Expected: FAIL with `AttributeError: gatt_discover`.

- [ ] **Step 3: Implement `gatt_discover`**

In `python/feralrf/radio.py`, inside class `Radio`, after `conn_status`, add:

```python
    def gatt_discover(self, timeout: float = 15.0) -> GattDiscoveryResult:
        """Issue CMD_GATT_DISCOVER and collect the streamed services + chars.

        Firmware responds with:
            1. RSP_ACK (acknowledge discovery started)
            2. Interleaved RSP_GATT_SERVICE / RSP_GATT_CHAR
            3. RSP_GATT_DONE with status byte
        RSP_CONN_STATUS frames (from a parallel poll) are ignored.

        Args:
            timeout: Absolute upper bound in seconds for the whole stream.
        """
        self._send_command(Command.GATT_DISCOVER, CommandBuilder.gatt_discover())

        # Consume initial ACK (or error) for the command itself.
        cmd_id, _seq, payload = self._read_response(
            timeout=min(timeout, 3.0),
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_DISCOVER failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to GATT_DISCOVER: 0x{cmd_id:02X}")

        services: list = []
        characteristics: list = []
        status = 0xFF
        deadline = time.monotonic() + timeout

        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            cmd_id, _seq, payload = self._read_response(
                timeout=remaining,
                expected={
                    Response.GATT_SERVICE,
                    Response.GATT_CHAR,
                    Response.GATT_DONE,
                    Response.CONN_STATUS,
                    Response.ERROR,
                },
            )

            if cmd_id == Response.GATT_SERVICE:
                if len(payload) < 6:
                    raise ProtocolError(f"GATT_SERVICE payload too short: {len(payload)}")
                start_h = int.from_bytes(payload[0:2], "little")
                end_h = int.from_bytes(payload[2:4], "little")
                services.append(GattService(start_handle=start_h, end_handle=end_h, uuid=bytes(payload[4:])))
            elif cmd_id == Response.GATT_CHAR:
                if len(payload) < 7:
                    raise ProtocolError(f"GATT_CHAR payload too short: {len(payload)}")
                handle = int.from_bytes(payload[0:2], "little")
                props = payload[2]
                val_handle = int.from_bytes(payload[3:5], "little")
                characteristics.append(
                    GattCharacteristic(
                        handle=handle, properties=props,
                        value_handle=val_handle, uuid=bytes(payload[5:]),
                    )
                )
            elif cmd_id == Response.GATT_DONE:
                status = payload[0] if payload else 0xFF
                break
            elif cmd_id == Response.CONN_STATUS:
                # Parallel poll from host app; ignore during stream.
                continue
            elif cmd_id == Response.ERROR:
                raise CommandError("GATT_DISCOVER stream error", payload[0] if payload else 0)

        return GattDiscoveryResult(
            services=services, characteristics=characteristics, status=status,
        )
```

**Note:** `_read_response()` requires `expected` to be a set of ints. Passing enum members works because they subclass `int`. Also, during the stream the firmware emits responses with `seq=0xFF` (no matching request seq). `_read_response` already has a branch that treats `seq=0xFF` as async and continues — we need to bypass that for the GATT stream. To keep the change small, use `self._serial` directly here if the skip-by-seq behavior masks GATT events. Verify in Step 4; if the test fails because `seq=0xFF` is being filtered out, add a `_read_stream_response()` helper that preserves seq=0xFF frames and refactor `gatt_discover` to use it.

- [ ] **Step 4: Run the tests and confirm they pass**

Run:
```bash
pytest tests/test_gatt_api.py -v
```

Expected: all tests PASS. If the stream-seq filter blocks `seq=0xFF` responses, implement the `_read_stream_response` helper referenced above (keep the same parser but skip the `if seq == 0xFF: warn; continue` branch for the expected stream cmd_ids). Re-run.

- [ ] **Step 5: Commit**

Run:
```bash
git add python/feralrf/radio.py python/tests/test_gatt_api.py
git commit -m "feat(f8): add Radio.gatt_discover() with streaming parser"
```

---

### Task 8: Implement `Radio.gatt_read()` and `Radio.gatt_write()`

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_gatt_api.py`

Per firmware, both commands first receive an `RSP_ACK`; `gatt_read` then receives `RSP_GATT_READ_VALUE` with `[handle:2 LE][value:N]`, and `gatt_write` receives `RSP_GATT_DONE` with `[status:1]`.

- [ ] **Step 1: Write the failing test**

Append to `python/tests/test_gatt_api.py`:

```python
def test_gatt_read_returns_value_bytes():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ACK, seq=0)
    # Read response: handle 0x0003, value b"Device42"
    fake.queue_response(
        Response.GATT_READ_VALUE,
        seq=0xFF,
        payload=struct.pack("<H", 0x0003) + b"Device42",
    )
    value = radio.gatt_read(0x0003, timeout=3.0)
    assert value == b"Device42"


def test_gatt_write_returns_status_byte():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ACK, seq=0)
    fake.queue_response(Response.GATT_DONE, seq=0xFF, payload=b"\x00")
    status = radio.gatt_write(0x0010, b"\xDE\xAD", timeout=3.0)
    assert status == 0
```

- [ ] **Step 2: Run and confirm it fails**

Run:
```bash
pytest tests/test_gatt_api.py::test_gatt_read_returns_value_bytes -v
```

Expected: FAIL with `AttributeError: gatt_read`.

- [ ] **Step 3: Implement both methods**

In `python/feralrf/radio.py`, inside class `Radio`, after `gatt_discover`, add:

```python
    def gatt_read(self, handle: int, timeout: float = 5.0) -> bytes:
        """Issue CMD_GATT_READ for the given attribute handle; return the value bytes."""
        self._send_command(Command.GATT_READ, CommandBuilder.gatt_read(handle))

        cmd_id, _seq, payload = self._read_response(
            timeout=min(timeout, 3.0),
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_READ failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to GATT_READ: 0x{cmd_id:02X}")

        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.GATT_READ_VALUE, Response.GATT_DONE, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_READ value error", payload[0] if payload else 0)
        if cmd_id == Response.GATT_DONE:
            status = payload[0] if payload else 0xFF
            raise CommandError("GATT_READ done without value", status)
        if len(payload) < 2:
            raise ProtocolError(f"GATT_READ_VALUE payload too short: {len(payload)}")
        return bytes(payload[2:])

    def gatt_write(self, handle: int, data: bytes, timeout: float = 5.0) -> int:
        """Issue CMD_GATT_WRITE; return the firmware status byte (0 = OK)."""
        self._send_command(Command.GATT_WRITE, CommandBuilder.gatt_write(handle, data))

        cmd_id, _seq, payload = self._read_response(
            timeout=min(timeout, 3.0),
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_WRITE failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to GATT_WRITE: 0x{cmd_id:02X}")

        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.GATT_DONE, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_WRITE ack error", payload[0] if payload else 0)
        return payload[0] if payload else 0xFF
```

- [ ] **Step 4: Run the tests and confirm they pass**

Run:
```bash
pytest tests/test_gatt_api.py -v
```

Expected: all tests PASS (12+ tests total).

- [ ] **Step 5: Commit**

Run:
```bash
git add python/feralrf/radio.py python/tests/test_gatt_api.py
git commit -m "feat(f8): add Radio.gatt_read() and Radio.gatt_write()"
```

---

### Task 9: Update `Radio` public API status (STABLE_METHODS, PENDING_FEATURES)

**Files:**
- Modify: `python/feralrf/radio.py`

The class docstring and `STABLE_METHODS` / `PENDING_FEATURES` tuples must reflect that GATT is now exposed.

- [ ] **Step 1: Edit the `Radio` docstring**

In `python/feralrf/radio.py`, update the `Radio` class docstring block so the "Stable" list includes `ble_connect`, `ble_disconnect`, `conn_status`, `gatt_discover`, `gatt_read`, `gatt_write`, and remove `gatt_discovery` from "Pending".

- [ ] **Step 2: Extend `STABLE_METHODS` tuple**

Add the six new method names to the `STABLE_METHODS` tuple (preserve existing entries and ordering style).

- [ ] **Step 3: Update `PENDING_FEATURES`**

Remove `"gatt_discovery"` from `PENDING_FEATURES`. Keep `"spectrum"` and `"initiator_mode"` (which now means something different; we'll revisit in F12).

- [ ] **Step 4: Add a contract test**

Append to `python/tests/test_gatt_api.py`:

```python
def test_gatt_methods_are_declared_stable():
    assert "ble_connect" in Radio.STABLE_METHODS
    assert "ble_disconnect" in Radio.STABLE_METHODS
    assert "conn_status" in Radio.STABLE_METHODS
    assert "gatt_discover" in Radio.STABLE_METHODS
    assert "gatt_read" in Radio.STABLE_METHODS
    assert "gatt_write" in Radio.STABLE_METHODS
    assert "gatt_discovery" not in Radio.PENDING_FEATURES
```

- [ ] **Step 5: Run the test and confirm it passes**

Run:
```bash
pytest tests/test_gatt_api.py::test_gatt_methods_are_declared_stable -v
```

Expected: PASS.

- [ ] **Step 6: Commit**

Run:
```bash
git add python/feralrf/radio.py python/tests/test_gatt_api.py
git commit -m "docs(f8): mark BLE connection and GATT methods as stable in Radio API"
```

---

### Task 10: Port `test_connect.py` to `demo_ble_connect_gatt.py` using the Radio API

**Files:**
- Create: `python/examples/lab/demo_ble_connect_gatt.py`
- Delete: `test_connect.py`

The refactor must preserve the interactive UX of `test_connect.py` (scan → pick connectable → connect → discover → optional `--read` → disconnect) but use `Radio` methods instead of hand-rolled COBS bytes. Known UUID and properties tables are re-used.

- [ ] **Step 1: Write the new demo**

Create `python/examples/lab/demo_ble_connect_gatt.py`:

```python
#!/usr/bin/env python3
"""FeralRF — BLE connect + GATT discovery demo (F8 validation).

Usage:
    python demo_ble_connect_gatt.py               # scan + interactive pick
    python demo_ble_connect_gatt.py <addr>        # connect to address (random by default)
    python demo_ble_connect_gatt.py <addr> 0      # connect to address (public)
    python demo_ble_connect_gatt.py <addr> <type> --read  # also read first readable char
"""

import struct
import sys
import time
from typing import Optional

from feralrf import Radio
from feralrf.enums import PHY


KNOWN_UUIDS = {
    0x1800: "Generic Access", 0x1801: "Generic Attribute",
    0x180A: "Device Information", 0x180F: "Battery Service",
    0x1810: "Blood Pressure", 0x1812: "Human Interface Device",
    0x1816: "Cycling Speed and Cadence", 0x1818: "Cycling Power",
    0x181C: "User Data", 0x1848: "Media Control Service",
    0xFE2C: "Google Fast Pair",
    0x2A00: "Device Name", 0x2A01: "Appearance",
    0x2A04: "Peripheral Preferred Conn Params", 0x2A05: "Service Changed",
    0x2A19: "Battery Level",
    0x2A24: "Model Number String", 0x2A25: "Serial Number String",
    0x2A26: "Firmware Revision String", 0x2A27: "Hardware Revision String",
    0x2A28: "Software Revision String", 0x2A29: "Manufacturer Name String",
    0x2A50: "PnP ID",
}

CHAR_PROPS = {
    0x01: "Broadcast", 0x02: "Read", 0x04: "WriteNoRsp", 0x08: "Write",
    0x10: "Notify", 0x20: "Indicate", 0x40: "AuthWrite", 0x80: "ExtProps",
}


def uuid_str(uuid_bytes: bytes) -> str:
    if len(uuid_bytes) == 2:
        val = struct.unpack("<H", uuid_bytes)[0]
        name = KNOWN_UUIDS.get(val, "")
        return f"0x{val:04X} ({name})" if name else f"0x{val:04X}"
    if len(uuid_bytes) == 16:
        b = uuid_bytes[::-1]
        return (f"{b[0:4].hex()}-{b[4:6].hex()}-{b[6:8].hex()}-"
                f"{b[8:10].hex()}-{b[10:16].hex()}")
    return uuid_bytes.hex()


def props_str(props: int) -> str:
    flags = [name for bit, name in CHAR_PROPS.items() if props & bit]
    return "|".join(flags) if flags else "None"


def parse_addr_arg(arg: str) -> bytes:
    """Accept 'AA:BB:CC:DD:EE:FF' or 'AABBCCDDEEFF', return 6 LE bytes."""
    clean = arg.replace(":", "").strip()
    if len(clean) != 12:
        raise SystemExit("ERROR: address must be 6 bytes (12 hex chars)")
    raw = bytes.fromhex(clean)
    return raw[::-1]


def scan_and_pick(radio: Radio, duration: float = 5.0) -> Optional[tuple]:
    """Scan ch37 for connectable advertisers; return (addr_le, addr_type)."""
    radio.set_phy(PHY.BLE_1M, channel=37)
    radio.start_rx()
    print(f"Scanning BLE ch37 for {duration:.0f}s...")
    devices: dict = {}
    deadline = time.monotonic() + duration

    for pkt in radio.read_packets(timeout=duration):
        if time.monotonic() > deadline:
            break
        if not pkt.crc_ok or not pkt.data or len(pkt.data) < 8:
            continue
        pdu_type = pkt.data[0] & 0x0F
        tx_add = (pkt.data[0] >> 6) & 1
        if pdu_type not in (0, 1, 2, 6):
            continue
        addr_le = pkt.data[2:8]
        key = bytes(addr_le)
        name = ""
        i = 8
        while i + 1 < len(pkt.data):
            ad_len = pkt.data[i]
            if ad_len == 0 or i + 1 + ad_len > len(pkt.data):
                break
            ad_type = pkt.data[i + 1]
            if ad_type in (0x08, 0x09):
                try:
                    name = pkt.data[i + 2:i + 1 + ad_len].decode("utf-8", errors="replace")
                except Exception:
                    pass
            i += 1 + ad_len
        connectable = pdu_type in (0, 1)
        entry = (addr_le, tx_add, pkt.rssi_dbm, name, connectable)
        if key not in devices or pkt.rssi_dbm > devices[key][2]:
            devices[key] = entry

    radio.stop_rx()

    if not devices:
        print("No BLE advertisers found.")
        return None

    items = list(devices.values())
    connectable_idx = [i for i, e in enumerate(items) if e[4]]
    print(f"\nFound {len(items)} device(s):")
    for i, (addr, atype, rssi, name, conn) in enumerate(items):
        addr_str = ":".join(f"{b:02X}" for b in reversed(addr))
        type_str = "random" if atype else "public"
        conn_str = "CONN" if conn else "non-conn"
        line = f"  [{i}] {addr_str} ({type_str}, {conn_str}) RSSI={rssi}"
        if name:
            line += f'  "{name}"'
        print(line)

    if not connectable_idx:
        print("\nNo connectable advertisers found.")
        return None

    try:
        choice = input(f"\nSelect device to connect [{connectable_idx[0]}]: ").strip()
        idx = int(choice) if choice else connectable_idx[0]
    except EOFError:
        idx = connectable_idx[0]
        print(f"(auto-selected [{idx}])")
    addr_le, addr_type, _, _, _ = items[idx]
    return bytes(addr_le), addr_type


def run(argv: list) -> int:
    do_read = "--read" in argv
    positional = [a for a in argv if not a.startswith("--")]

    addr_le: Optional[bytes] = None
    addr_type = 1

    with Radio() as radio:
        radio.init()

        if len(positional) >= 1:
            addr_le = parse_addr_arg(positional[0])
            addr_type = int(positional[1]) if len(positional) >= 2 else 1
        else:
            pick = scan_and_pick(radio)
            if pick is None:
                return 1
            addr_le, addr_type = pick

        addr_str = ":".join(f"{b:02X}" for b in reversed(addr_le))
        print(f"\nConnecting to {addr_str} (type={'random' if addr_type else 'public'})")

        result = radio.ble_connect(addr_le, addr_type, timeout=8.0)
        results = {0: "OK", 1: "TIMEOUT", 2: "NO_SYNC", 3: "RF_ERR"}
        print(f"Connection result: {results.get(result.result, f'0x{result.result:02X}')}")
        if not result.is_ok:
            return 2

        time.sleep(2.0)
        status = radio.conn_status()
        print(f"  connected={status.connected} events={status.events} "
              f"att_state={status.att_state} last_status=0x{status.last_status:04X}")
        if not status.connected:
            print("Connection dropped before GATT.")
            return 3

        print("\n=== GATT Discovery ===")
        discovery = radio.gatt_discover(timeout=30.0)

        print(f"\nServices ({len(discovery.services)}):")
        for svc in discovery.services:
            print(f"  0x{svc.start_handle:04X}-0x{svc.end_handle:04X}  UUID={uuid_str(svc.uuid)}")
        print(f"\nCharacteristics ({len(discovery.characteristics)}):")
        for ch in discovery.characteristics:
            print(f"  decl=0x{ch.handle:04X} val=0x{ch.value_handle:04X}  "
                  f"props=[{props_str(ch.properties)}]  UUID={uuid_str(ch.uuid)}")
        print(f"\nGATT done (status={discovery.status})")

        if do_read and discovery.characteristics:
            for ch in discovery.characteristics:
                if ch.properties & 0x02:
                    print(f"\nReading 0x{ch.value_handle:04X} ({uuid_str(ch.uuid)})...")
                    value = radio.gatt_read(ch.value_handle)
                    try:
                        print(f"  UTF-8: {value.decode('utf-8')!r}")
                    except UnicodeDecodeError:
                        print(f"  hex:   {value.hex()}")
                    break

        print("\nDisconnecting...")
        radio.ble_disconnect()
        print("Disconnected.")
    return 0


if __name__ == "__main__":
    sys.exit(run(sys.argv[1:]))
```

- [ ] **Step 2: Delete the old script and commit the move**

Run:
```bash
git rm --cached test_connect.py 2>/dev/null || rm -f test_connect.py
git add python/examples/lab/demo_ble_connect_gatt.py
git add -u  # picks up test_connect.py deletion
git status
```

Expected: `demo_ble_connect_gatt.py` new, `test_connect.py` deleted (or just removed from untracked list).

- [ ] **Step 3: Verify the demo imports cleanly (no hardware required)**

Run:
```bash
python -c "import ast; ast.parse(open('python/examples/lab/demo_ble_connect_gatt.py').read()); print('OK')"
python -c "from feralrf import Radio; r = Radio(port='/dev/null'); print(hasattr(r, 'ble_connect'), hasattr(r, 'gatt_discover'))"
```

Expected: both commands print `OK` / `True True`.

- [ ] **Step 4: Commit**

Run:
```bash
git commit -m "feat(f8): port test_connect.py to demo_ble_connect_gatt.py using Radio API"
```

---

### Task 11: Register `hardware_ble` pytest marker and add the integration test

**Files:**
- Modify: `python/pyproject.toml`
- Create: `python/tests/test_gatt_integration.py`

The integration test is gated by the `hardware_ble` marker **and** by environment variables. Without a real device + peripheral, it is skipped cleanly.

- [ ] **Step 1: Register the marker**

In `python/pyproject.toml`, under `[tool.pytest.ini_options]`, add:

```toml
markers = [
    "hardware_ble: integration test requires a FeralRF device and a real BLE peripheral",
]
```

- [ ] **Step 2: Create the integration test**

Create `python/tests/test_gatt_integration.py`:

```python
"""FeralRF — GATT hardware-in-the-loop integration test.

Run with:
    FERALRF_PERIPHERAL_ADDR=AA:BB:CC:DD:EE:FF \
    FERALRF_PERIPHERAL_TYPE=1 \
    pytest tests/test_gatt_integration.py -m hardware_ble -v

Skipped unless the env vars are set.
"""

import os
import time

import pytest

from feralrf import Radio

pytestmark = pytest.mark.hardware_ble


ADDR = os.environ.get("FERALRF_PERIPHERAL_ADDR")
ATYPE = int(os.environ.get("FERALRF_PERIPHERAL_TYPE", "1"))

if not ADDR:
    pytest.skip("FERALRF_PERIPHERAL_ADDR not set", allow_module_level=True)


def _addr_le(s: str) -> bytes:
    return bytes(int(x, 16) for x in reversed(s.split(":")))


@pytest.fixture(scope="module")
def radio() -> Radio:
    r = Radio()
    r.init()
    yield r
    try:
        r.ble_disconnect(timeout=1.0)
    except Exception:
        pass
    r.disconnect()


def test_connect_discover_and_read_device_name(radio: Radio):
    result = radio.ble_connect(_addr_le(ADDR), ATYPE, timeout=8.0)
    assert result.is_ok, f"connect failed: result={result.result}"

    time.sleep(1.5)
    status = radio.conn_status()
    assert status.connected, f"connection dropped before discovery: {status}"

    discovery = radio.gatt_discover(timeout=20.0)
    assert len(discovery.services) >= 1, "no services discovered"
    assert len(discovery.characteristics) >= 1, "no characteristics discovered"
    assert discovery.status == 0, f"discovery status={discovery.status}"

    # Prefer Device Name (0x2A00) if present; else first readable char.
    target = None
    for ch in discovery.characteristics:
        if ch.uuid == b"\x00\x2A" and (ch.properties & 0x02):
            target = ch
            break
    if target is None:
        for ch in discovery.characteristics:
            if ch.properties & 0x02:
                target = ch
                break
    assert target is not None, "no readable characteristic found"

    value = radio.gatt_read(target.value_handle, timeout=5.0)
    assert len(value) > 0, "empty read value"


def test_disconnect_is_clean_and_allows_reconnect(radio: Radio):
    radio.ble_disconnect(timeout=2.0)

    # Second connect must succeed without a reset_device() in between.
    result = radio.ble_connect(_addr_le(ADDR), ATYPE, timeout=8.0)
    assert result.is_ok, f"reconnect failed: result={result.result}"
    radio.ble_disconnect(timeout=2.0)
```

- [ ] **Step 3: Confirm the test is discovered and skipped when env is missing**

Run:
```bash
pytest tests/test_gatt_integration.py -v
```

Expected: both tests skipped with reason `FERALRF_PERIPHERAL_ADDR not set`.

- [ ] **Step 4: Confirm the marker selection works**

Run:
```bash
pytest -m hardware_ble --collect-only
```

Expected: the two integration tests are listed, regular unit tests are excluded.

- [ ] **Step 5: Commit**

Run:
```bash
git add python/pyproject.toml python/tests/test_gatt_integration.py
git commit -m "test(f8): add hardware_ble integration test for GATT"
```

---

### Task 12: Flash current firmware and smoke-test the demo with a real peripheral (checkpoint humano)

**Hardware required:**
- 1× CatSniffer CC1352P7 board (one of the known-good three).
- 1× Real BLE peripheral (per D1; default assumption: smartphone + BLE Peripheral Simulator app).
- Host laptop.

**Note:** Firmware is the TI-RTOS build from `feature/ti-rtos-migration` head. If it's not already flashed on the board, flash it with catnip using the `.hex` (decision #17). Retry flash 2× on failure before asking for a manual reset (feedback memory).

- [ ] **Step 1: Build firmware**

Run:
```bash
cd firmware/cc1352 && mkdir -p build && cd build
cmake .. && make -j$(nproc)
ls -lh feralrf_cc1352.hex
```

Expected: `.hex` produced, size <120 KB.

- [ ] **Step 2: Flash CC1352**

Run (adjust the port to the catnip-recognized device):
```bash
catnip -f firmware/cc1352/build/feralrf_cc1352.hex -p /dev/ttyACM0
```

Expected: catnip reports success. If it fails, retry once more; if still fails, stop and ask the user to hold the board's reset button.

- [ ] **Step 3: Start the BLE peripheral on the chosen target**

Manual action:
- On the smartphone (or ESP32/Pi), start the BLE peripheral app and make sure it is advertising as connectable on channel 37.
- Note the advertised MAC address and type (random vs public).

- [ ] **Step 4: Run the demo in scan-and-pick mode**

Run:
```bash
cd python && source .venv/bin/activate
python examples/lab/demo_ble_connect_gatt.py
```

Expected output includes:
- The target's MAC with a reasonable RSSI (>-80 dBm at 1 m).
- `Connection result: OK`.
- `connected=True events=<n>` shortly after connect.
- At least one service and one characteristic listed.
- `GATT done (status=0)`.
- `Disconnected.`

- [ ] **Step 5: Run the demo a second time back-to-back without resetting the board**

Run the same command again immediately.

Expected: same successful flow. If the second run fails, record the failure; this indicates a disconnect cleanup bug (not an F8 pass).

- [ ] **Step 6: Run the demo with `--read` to validate `gatt_read`**

Run:
```bash
python examples/lab/demo_ble_connect_gatt.py --read
```

Expected: same flow, plus a "Reading 0x...." line followed by a UTF-8 or hex value. If the peripheral exposes Device Name (0x2A00), the printed string should match what the peripheral advertises.

- [ ] **Step 7: Run the integration test end-to-end**

Run:
```bash
# From the MAC printed in Step 4
FERALRF_PERIPHERAL_ADDR=AA:BB:CC:DD:EE:FF \
FERALRF_PERIPHERAL_TYPE=1 \
pytest tests/test_gatt_integration.py -m hardware_ble -v
```

Expected: both integration tests PASS.

- [ ] **Step 8: Capture evidence**

Save the terminal output of Steps 4, 5, 6, and 7 (e.g. with `script -c ... output.txt` or copy-paste into a text file). Keep the evidence file somewhere outside the repo; we'll link to a summary in Task 14.

- [ ] **Step 9 (optional, only on failure)**

If Step 4 fails with a timeout on connect:
- Check `att_state` in the status response; if it stays in IDLE (0), firmware never drove connect.
- Check that the target is still advertising on channel 37.
- Power-cycle CC1352 (`radio.reset_device()`) and retry.

If Step 5 fails (second connect), investigate ATT MTU + L2CAP SDU handling (R2 in spec risks) — that's an F8 blocker and likely needs a firmware-side fix. Record the failure and open a follow-up fix before claiming F8 green.

---

### Task 13: Repeat the checkpoint on a second peripheral (variety check)

**Hardware required:**
- A second BLE peripheral type (if the first was a smartphone, use an ESP32/nRF52840 with a GATT server; or vice versa).

- [ ] **Step 1: Flash (or boot) a second peripheral**

Whatever target was not used in Task 12 — bring it up as a GATT server.

- [ ] **Step 2: Run scan-and-pick against the new peripheral**

Run:
```bash
python examples/lab/demo_ble_connect_gatt.py
```

Expected: the new peripheral appears in the scan, connects, discovery lists the GATT services it actually publishes.

- [ ] **Step 3: Run the `--read` variant**

Run:
```bash
python examples/lab/demo_ble_connect_gatt.py --read
```

Expected: successful read on a readable characteristic.

- [ ] **Step 4: Capture evidence**

Append the output of Steps 2 and 3 to the evidence file started in Task 12. Include the peripheral name and which device it is (e.g. "ESP32-S3, firmware = bluefruit-example-gatt-server").

---

### Task 14: Document F8 closure in the spec and update PYTHON_API

**Files:**
- Modify: `docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md`
- Modify: `docs/PYTHON_API.md`

- [ ] **Step 1: Mark F8 done in the spec**

Edit `docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md`:
- In the phases table in §5, change F8's status emoji from 🔜 to ✅.
- In §5 F8 block, add a short "Cierre" subsection under the existing content:

```markdown
**Cierre (2026-MM-DD):** GATT validado contra <peripheral #1> y <peripheral #2>.
Evidencia: <short link or filename>. Tag `v2.0-f8` at commit `<shortsha>`.
```

Replace `<peripheral #1>`, `<peripheral #2>`, date, filename, and shortsha with the real values from Tasks 12-13.

- [ ] **Step 2: Add the GATT section to PYTHON_API.md**

In `docs/PYTHON_API.md`, add a new section titled "BLE central + GATT" with examples:

```markdown
## BLE central + GATT

Requires a firmware built from `feature/ti-rtos-migration` (TI-RTOS) at
commit `v2.0-f8` or later.

```python
from feralrf import Radio
from feralrf.enums import PHY

with Radio() as radio:
    radio.init()
    radio.set_phy(PHY.BLE_1M, channel=37)

    addr_le = bytes.fromhex("FFEEDDCCBBAA")[::-1]  # AA:BB:CC:DD:EE:FF
    result = radio.ble_connect(addr_le, addr_type=1)
    assert result.is_ok

    discovery = radio.gatt_discover(timeout=30.0)
    for svc in discovery.services:
        print(svc.start_handle, svc.end_handle, svc.uuid.hex())

    # Read Device Name (assumes discovery found UUID 0x2A00)
    value = radio.gatt_read(handle=0x0003)
    print(value.decode("utf-8", errors="replace"))

    radio.ble_disconnect()
```
```

- [ ] **Step 3: Commit docs**

Run:
```bash
git add docs/superpowers/specs/2026-04-24-feralrf-plan-v2-design.md docs/PYTHON_API.md
git commit -m "docs(f8): close F8 in spec and document GATT API"
```

---

### Task 15: Tag `v2.0-f8` and hand off to the user for merge

**Files:** none modified.

- [ ] **Step 1: Run the full Python test suite one more time**

Run:
```bash
cd python && pytest -v
```

Expected: all unit tests PASS; integration tests skipped (unless env vars are set).

- [ ] **Step 2: Create the tag**

Run:
```bash
git tag -a v2.0-f8 -m "F8: Validate GATT end-to-end"
git show v2.0-f8 --stat --no-patch
```

Expected: tag points at the last docs commit.

- [ ] **Step 3: Report to user**

Produce a short status message (do NOT push):
- Branch: `feature/f8-gatt-validation`
- Tag: `v2.0-f8`
- Commits in the branch: list with `git log --oneline feature/ti-rtos-migration..HEAD`
- Evidence file location and summary (from Tasks 12-13).
- Any contingent fixes that had to be applied (MTU, L2CAP, seq=0xFF filter).
- Open questions for the merge decision (squash vs rebase, merge now or wait until F9 done first).

End of plan. Do not start F9 until the user approves merging F8.

---

## Contingent tasks (only if validation surfaces a blocker)

These are not mandatory. Execute **only** if Task 12 or Task 13 surfaces the matching failure mode.

### Contingent C1 — Stream seq=0xFF filter bypass

**Trigger:** `gatt_discover` unit test fails because `_read_response` drops `seq=0xFF` frames as "async RF error".

**Action:** Add a `_read_stream_response()` helper in `radio.py` that behaves like `_read_response` except it does NOT treat `seq=0xFF` as async. Use it inside `gatt_discover`, `gatt_read` (for the value frame), and `gatt_write` (for the DONE frame). Keep the existing `_read_response` unchanged to preserve behavior for RADIO_INIT / SET_PHY / SET_CHANNEL etc.

### Contingent C2 — ATT MTU exchange

**Trigger:** Characteristic reads return truncated bytes (length = 20, exactly ATT_MTU-3 with default MTU=23), OR a write larger than 20 bytes fails with `GATT_DONE status != 0`.

**Action:** Firmware change. In `att_client.c`, issue an Exchange MTU request (ATT opcode 0x02) immediately after the connection reaches IDLE and before the first GATT_DISCOVER. Set the client MTU to 247. Update the PDU length requests over LL similarly. This is a firmware-only change and must go through a separate commit referencing this contingency.

### Contingent C3 — L2CAP SDU length routing

**Trigger:** `RSP_ERROR` with error code 0x05 (INVALID_STATE) during GATT read/write on characteristics ≥ MTU bytes.

**Action:** In `tx_queue.c`, verify LLID 2 (start of L2CAP SDU) carries the SDU length prefix and LLID 1 (continuation) does not. If the current routing is wrong, fix the LLID selection per BT spec §6.3.

---

## Self-Review

Checked against spec §5 F8 ("Validar GATT end-to-end"):

- **Entregable: commit `test_connect.py` refactored into `python/examples/lab/`** → Task 10 creates `demo_ble_connect_gatt.py` and deletes the original. ✅
- **Entregable: Python radio.py exposes `connect`, `gatt_discover`, `gatt_read`, `gatt_write`, `disconnect`** → Tasks 5-8 add `ble_connect`, `ble_disconnect`, `conn_status`, `gatt_discover`, `gatt_read`, `gatt_write` (named to avoid collision with existing `Radio.connect`/`Radio.disconnect` which manage the serial port). ✅
- **Criterio: discovery ≥1 service + ≥1 char** → Task 13 `test_connect_discover_and_read_device_name` asserts exactly this. ✅
- **Criterio: Device Name read** → Same integration test. ✅
- **Criterio: disconnect clean (reconnect without reset)** → Task 13 `test_disconnect_is_clean_and_allows_reconnect`. ✅
- **Criterio: `att_state` returns to IDLE after RSP_GATT_DONE** → Covered by `conn_status` snapshot in demo Step 4 and exposed on the `ConnectionStatus` dataclass. Manual check during checkpoint.
- **Checkpoint humano: peripheral real + 2 runs on same target + 2 peripherals** → Tasks 12 (same target 2 runs) + 13 (second peripheral). ✅
- **Risk R2 (ATT MTU) / L2CAP SDU** → Contingent tasks C2 / C3. ✅
- **No firmware changes in the main path** → Correct; firmware-only Contingencies are opt-in. ✅
- **Pending decisions D1/D2 resolved in Task 1 Step 3** → ✅.
