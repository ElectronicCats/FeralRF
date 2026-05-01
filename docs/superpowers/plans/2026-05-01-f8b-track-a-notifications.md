# F8b Track A — GATT Notifications + AttClient Stale-State Fix

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship ATT notification + indication reception (`RSP_GATT_NOTIFY`)
in firmware and Python, plus root-cause fix for the AttClient
stale-state bug that breaks reconnect-after-disconnect cycles.

**Architecture:** Track 0 (AttClient bug) is sequenced first — without
it the smoke validation for notifications would require `reset_device()`
between each run. Track A then adds two new ATT incoming opcode
handlers (0x1B NOTIFY, 0x1D INDICATE) in `att_client.c`, a new
`CMD_GATT_SUBSCRIBE` dispatch in `command_processor.c`, and the host
API surface (`gatt_subscribe`, `read_gatt_notifications`,
`GattNotification` dataclass).

**Tech Stack:** C (CC1352 firmware, TI-RTOS 7, SDK 7.10), Python 3.11
(feralrf package, pyserial, pytest), COBS-framed binary protocol.

**Spec:** `docs/superpowers/specs/2026-05-01-f8b-gatt-investigation-toolkit-design.md`

**Bug context:** `memory/project_gatt_attclient_bug.md`

**Smoke target:** Sony WH-CH720N at MAC `A8:E6:E8:8A:7D:F8` on
`/dev/ttyACM2` (per `docs/investigations/2026-05-01-sony-wh-ch720n.md`).

---

## File structure

### Files to CREATE

| Path | Responsibility |
|------|----------------|
| `python/tests/test_gatt_notifications.py` | Hardware-free unit tests for subscribe + GattNotification + read_gatt_notifications iterator |
| `python/examples/lab/smoke_f8b_notifications.py` | Hardware smoke against Sony WH-CH720N — subscribe + button trigger + capture |
| `python/examples/lab/diag_attclient_repro.py` | Deterministic repro of the AttClient stale-state bug (used in Track 0) |

### Files to MODIFY

| Path | What changes |
|------|--------------|
| `firmware/cc1352/src/att_client.c` | Add ATT opcode 0x1B + 0x1D handlers + `AttClient_emitNotification` + AttClient stale-state fix (Track 0) |
| `firmware/cc1352/include/att_client.h` | Declare `AttClient_sendCfm` + any new public fns |
| `firmware/cc1352/src/command_processor.c` | Add `CMD_GATT_SUBSCRIBE` (0x46) dispatch + `RSP_GATT_NOTIFY` (0x95) opcode constant |
| `python/feralrf/commands.py` | Add `Command.GATT_SUBSCRIBE = 0x46` enum + `CommandBuilder.gatt_subscribe(handle, enable)` |
| `python/feralrf/responses.py` | Add `Response.GATT_NOTIFY = 0x95` enum |
| `python/feralrf/radio.py` | Add `GattNotification` dataclass + `gatt_subscribe()` + `read_gatt_notifications()` methods |

---

## Track 0 — AttClient stale-state bug

### Task 0.1: Write deterministic repro script

**Files:**
- Create: `python/examples/lab/diag_attclient_repro.py`

- [ ] **Step 1: Write the repro script**

```python
#!/usr/bin/env python3
"""Deterministic repro: ble_connect → gatt_discover → ble_disconnect →
ble_connect → gatt_discover. Second discover fails with timeout per
memory/project_gatt_attclient_bug.md.

Usage: python diag_attclient_repro.py [port] [target_mac]
"""
import sys
import time

from feralrf import Radio

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM2"
target_mac = sys.argv[2] if len(sys.argv) > 2 else "A8:E6:E8:8A:7D:F8"
addr_le = bytes.fromhex("".join(target_mac.split(":")[::-1]))

r = Radio(port)
r.connect()
time.sleep(0.3)
r.init()

for cycle in range(1, 6):
    print(f"\n=== cycle {cycle}/5 ===")
    print("  ble_connect")
    res = r.ble_connect(addr_le, addr_type=0, timeout=10.0)
    print(f"    result code = {res.result}")
    if res.result != 0:
        print("    CONNECT FAILED — aborting cycle")
        break
    time.sleep(0.5)
    print("  gatt_discover")
    try:
        d = r.gatt_discover(timeout=15.0)
        print(f"    OK — services={len(d.services)} chars={len(d.characteristics)}")
    except Exception as e:
        print(f"    FAILED: {type(e).__name__}: {e}")
    print("  ble_disconnect")
    try:
        r.ble_disconnect()
    except Exception:
        pass
    time.sleep(1.0)

r.disconnect()
```

- [ ] **Step 2: Run, confirm bug reproduces**

```bash
cd python && source .venv/bin/activate
python examples/lab/diag_attclient_repro.py /dev/ttyACM2
```

Expected: cycle 1 succeeds, cycles 2-5 fail with `gatt_discover` timeout. This confirms the documented bug pattern.

- [ ] **Step 3: Commit the repro script**

```bash
git add python/examples/lab/diag_attclient_repro.py
git commit -m "diag: deterministic repro for AttClient stale-state bug"
```

---

### Task 0.2: Instrument firmware to capture state at the failure point

**Files:**
- Modify: `firmware/cc1352/src/att_client.c`

- [ ] **Step 1: Add diagnostic logging at every public AttClient entry/exit**

Find the existing `AttClient_startDiscover()`, `AttClient_startRead()`, `AttClient_startWrite()`, and any disconnect handler in `att_client.c`. At entry log: `s_state`, `s_mtu`, any handle ranges, the active connection handle. At exit log: success/fail + final state.

Concrete pattern (mirror existing log macros — search for `LOG_INFO` or `printf` already used in the file):

```c
static void att_log(const char *tag) {
    /* Add at function entries — replace LOG_DBG with whatever the file already uses */
    LOG_DBG("ATT %s: state=%d mtu=%u conn_alive=%d",
            tag, (int)s_state, (unsigned)s_mtu, (int)BleConn_isConnected());
}
```

Add `att_log("startDiscover-enter")` etc. at every entry/exit.

- [ ] **Step 2: Build firmware**

```bash
cd firmware/cc1352/build && cmake --build . -j$(nproc)
```

Expected: clean build, no new warnings.

- [ ] **Step 3: Flash both boards**

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
HEX=/home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
python -m catnip flash -d 1 "$HEX"  # retry up to 2x per memory rule
```

- [ ] **Step 4: Run repro with serial debug capture**

In another terminal, capture firmware UART debug output:

```bash
# UART debug stream — adjust port if firmware logs to a different one
cat /dev/ttyACM4 &
```

Then run the repro:

```bash
cd python && python examples/lab/diag_attclient_repro.py /dev/ttyACM2
```

- [ ] **Step 5: Inspect logs at cycle 1 success vs cycle 2 failure**

Compare `s_state` values between successful cycle 1 and failed cycle 2. Document findings inline in `memory/project_gatt_attclient_bug.md` with timestamps and observed state machine values.

---

### Task 0.3: Apply the fix

**Files:**
- Modify: `firmware/cc1352/src/att_client.c`

The fix shape depends on Task 0.2 findings. The three most likely root-cause classes are:

(a) **State variable not reset on disconnect.** Some `s_state`, `s_pending_discovery`, or `s_active_seq` retains a stale value across the disconnect → connect transition. Fix: hook the disconnect callback (or `AttClient_init`) to reset the relevant statics.

(b) **Pending command never finalized.** A discovery in flight when disconnect happens never receives its DONE event, leaving the AttClient blocked. Fix: on disconnect, abort any in-flight ATT operation cleanly.

(c) **AttClient_init not called on second connect.** The initialization is one-shot at boot. Fix: re-init the AttClient module on each `BleConn_connected` event.

- [ ] **Step 1: Pick the fix shape per Task 0.2 findings**

Document the chosen class (a/b/c or other) inline in the commit message and in `memory/project_gatt_attclient_bug.md`.

- [ ] **Step 2: Implement the fix**

For class (a), example skeleton:

```c
/* In att_client.c — add a reset function called on disconnect */
void AttClient_handleDisconnect(void) {
    s_state = ATT_STATE_IDLE;
    s_active_seq = 0;
    s_pending_discovery = false;
    s_mtu = 23;  /* default ATT MTU */
    /* clear any pending response cache */
}
```

Then hook this into the BLE central disconnect path. Find where `BleConn_disconnected()` is signaled and add `AttClient_handleDisconnect()` to the callback list.

For class (b) or (c), the fix shape is similar — restore the identified-stale state at the right lifecycle hook.

- [ ] **Step 3: Build firmware**

```bash
cd firmware/cc1352/build && cmake --build . -j$(nproc)
```

Expected: clean build.

- [ ] **Step 4: Flash and re-run repro**

```bash
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
python -m catnip flash -d 1 "$HEX"
cd /home/sabas/Documents/electroniccats/FeralRF/python
python examples/lab/diag_attclient_repro.py /dev/ttyACM2
```

Expected: 5/5 cycles all succeed. Each gatt_discover returns 13 services, 47 characteristics consistently.

- [ ] **Step 5: Run the repro 3 times (separate processes) to confirm reliability**

```bash
for i in 1 2 3; do
    echo "--- run $i ---"
    python examples/lab/diag_attclient_repro.py /dev/ttyACM2 || break
done
```

Expected: 3/3 runs report "5/5 cycles" each.

- [ ] **Step 6: Update memory and commit**

```bash
# Update memory/project_gatt_attclient_bug.md to mark RESOLVED with commit SHA placeholder
# Then commit

git add firmware/cc1352/src/att_client.c
git commit -m "fix(f8b): AttClient stale-state on reconnect — <root-cause-class>

5/5 reconnect cycles now succeed without reset_device().
Closes the bug filed in memory/project_gatt_attclient_bug.md.
"
```

After commit, update the memory file with the actual SHA:

```bash
SHA=$(git rev-parse --short HEAD)
# edit memory/project_gatt_attclient_bug.md to reference $SHA as the fix commit
```

---

### Task 0.4: Decision gate — fix landed or deferred

- [ ] **Step 1: Decision**

If Task 0.3 produced a clean fix and 3x5/5 cycles pass: continue to Track A.

If the root cause is deeper than F8b scope (e.g., requires RF driver redesign):
- Revert Task 0.3 changes
- Update `memory/project_gatt_attclient_bug.md` with explicit deferral statement and what was attempted
- Continue to Track A but include `r.reset_device()` in the smoke test prologue
- Document the deferral in this plan as a comment in Track A

- [ ] **Step 2: Push deferral to spec acceptance gate**

If deferred, edit `docs/superpowers/specs/2026-05-01-f8b-gatt-investigation-toolkit-design.md` "Acceptance gates for v2.0-f8b" item 8 to reflect: "fixed" or "deferred to F8c with documented status."

```bash
git add memory/project_gatt_attclient_bug.md docs/superpowers/specs/2026-05-01-f8b-gatt-investigation-toolkit-design.md
git commit -m "docs(f8b): AttClient bug status post-investigation"
```

---

## Track A — GATT Notifications

### Task A.1: Add command + response opcodes

**Files:**
- Modify: `python/feralrf/commands.py`
- Modify: `python/feralrf/responses.py`
- Modify: `firmware/cc1352/src/command_processor.c`

- [ ] **Step 1: Find the existing enums and constants**

```bash
grep -n "GATT_DISCOVER\|GATT_READ\|GATT_WRITE" python/feralrf/commands.py python/feralrf/responses.py firmware/cc1352/src/command_processor.c
```

Note the existing opcode values to confirm 0x46 (CMD_GATT_SUBSCRIBE) and 0x95 (RSP_GATT_NOTIFY) are unused.

- [ ] **Step 2: Add opcode in `python/feralrf/commands.py`**

In the `Command` enum, add:

```python
class Command(IntEnum):
    # ... existing entries ...
    GATT_SUBSCRIBE = 0x46
```

In `CommandBuilder` (the static class with builder methods), add:

```python
@staticmethod
def gatt_subscribe(handle: int, enable: bool, indicate: bool = False) -> bytes:
    """Build CMD_GATT_SUBSCRIBE payload: handle_le[2] + enable[1] + indicate[1]."""
    return handle.to_bytes(2, "little") + bytes([1 if enable else 0, 1 if indicate else 0])
```

- [ ] **Step 3: Add opcode in `python/feralrf/responses.py`**

```python
class Response(IntEnum):
    # ... existing entries ...
    GATT_NOTIFY = 0x95
```

- [ ] **Step 4: Add opcode in firmware `command_processor.c`**

Find the block of `#define CMD_GATT_*` macros. Add:

```c
#define CMD_GATT_SUBSCRIBE   0x46u
#define RSP_GATT_NOTIFY      0x95u
```

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/commands.py python/feralrf/responses.py firmware/cc1352/src/command_processor.c
git commit -m "feat(f8b-trackA): wire CMD_GATT_SUBSCRIBE + RSP_GATT_NOTIFY opcodes"
```

---

### Task A.2: Unit test for `gatt_subscribe` host-side

**Files:**
- Create: `python/tests/test_gatt_notifications.py`

- [ ] **Step 1: Write the failing tests**

```python
"""Unit tests for F8b Track A — GATT notifications host-side API.

Hardware-free; mocks the Radio _send_command + _read_response transport.
"""
from unittest.mock import MagicMock, patch

import pytest

from feralrf import Radio
from feralrf.commands import Command
from feralrf.responses import Response


@pytest.fixture
def radio_mock():
    """Radio instance with mocked transport methods."""
    r = Radio()
    r._send_command = MagicMock()
    r._read_response = MagicMock()
    r._serial = MagicMock()
    r._serial.is_open = True
    return r


def test_gatt_subscribe_writes_correct_command(radio_mock):
    """gatt_subscribe(212, enable=True) must send CMD_GATT_SUBSCRIBE
    with payload handle_le[2] + enable[1] + indicate[1]."""
    radio_mock._read_response.return_value = (Response.ACK, 0, b"")

    radio_mock.gatt_subscribe(handle=212, enable=True)

    # Verify _send_command call
    call_args = radio_mock._send_command.call_args
    cmd_id, payload = call_args[0]
    assert cmd_id == Command.GATT_SUBSCRIBE
    assert payload == b"\xd4\x00\x01\x00"  # 212 LE, enable=1, indicate=0


def test_gatt_subscribe_indicate_sets_indicate_byte(radio_mock):
    """gatt_subscribe(212, indicate=True) sets the indicate flag."""
    radio_mock._read_response.return_value = (Response.ACK, 0, b"")

    radio_mock.gatt_subscribe(handle=212, enable=True, indicate=True)

    cmd_id, payload = radio_mock._send_command.call_args[0]
    assert payload == b"\xd4\x00\x01\x01"


def test_gatt_subscribe_disable(radio_mock):
    """gatt_subscribe(212, enable=False) sets enable=0."""
    radio_mock._read_response.return_value = (Response.ACK, 0, b"")

    radio_mock.gatt_subscribe(handle=212, enable=False)

    cmd_id, payload = radio_mock._send_command.call_args[0]
    assert payload[2] == 0


def test_gatt_subscribe_raises_on_error_response(radio_mock):
    """If firmware returns RSP_ERROR, gatt_subscribe raises CommandError."""
    from feralrf.exceptions import CommandError
    radio_mock._read_response.return_value = (Response.ERROR, 0, b"\x05")  # ERR_INVALID_STATE

    with pytest.raises(CommandError):
        radio_mock.gatt_subscribe(handle=212, enable=True)
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
cd python && source .venv/bin/activate
pytest tests/test_gatt_notifications.py -v
```

Expected: 4 tests FAIL with `AttributeError: 'Radio' object has no attribute 'gatt_subscribe'`.

---

### Task A.3: Implement `gatt_subscribe` on Radio class

**Files:**
- Modify: `python/feralrf/radio.py`

- [ ] **Step 1: Locate where to add the method**

```bash
grep -n "def gatt_write\|def gatt_read" python/feralrf/radio.py
```

Add `gatt_subscribe` immediately after `gatt_write` (logical grouping with other GATT methods).

- [ ] **Step 2: Implement the method**

```python
def gatt_subscribe(
    self,
    handle: int,
    enable: bool = True,
    indicate: bool = False,
    timeout: float = 3.0,
) -> None:
    """Subscribe to notifications (or indications) for a GATT characteristic.

    Sends CMD_GATT_SUBSCRIBE which the firmware handles by writing
    0x0001 (notify) or 0x0002 (indicate) — or 0x0000 if disabling — to
    the CCC descriptor at (handle + 1).

    Args:
        handle: The characteristic VALUE handle (not declaration).
            Firmware writes the CCC at handle + 1 per BLE convention.
        enable: True to subscribe, False to unsubscribe.
        indicate: True for indications (0x0002), False for notifications (0x0001).
            Ignored when enable=False.
        timeout: Seconds to wait for ACK.
    """
    self._send_command(
        Command.GATT_SUBSCRIBE,
        CommandBuilder.gatt_subscribe(handle, enable, indicate),
    )
    cmd_id, _seq, payload = self._read_response(
        timeout=timeout,
        expected={Response.ACK, Response.ERROR},
    )
    if cmd_id == Response.ERROR:
        raise CommandError("GATT_SUBSCRIBE failed", payload[0] if payload else 0)
    if cmd_id != Response.ACK:
        raise ProtocolError(f"Unexpected response to GATT_SUBSCRIBE: 0x{cmd_id:02X}")
```

Add `"gatt_subscribe"` to the `STABLE_METHODS` tuple.

- [ ] **Step 3: Run tests to verify they pass**

```bash
pytest tests/test_gatt_notifications.py::test_gatt_subscribe_writes_correct_command tests/test_gatt_notifications.py::test_gatt_subscribe_indicate_sets_indicate_byte tests/test_gatt_notifications.py::test_gatt_subscribe_disable tests/test_gatt_notifications.py::test_gatt_subscribe_raises_on_error_response -v
```

Expected: 4 tests PASS.

- [ ] **Step 4: Commit**

```bash
git add python/feralrf/radio.py python/tests/test_gatt_notifications.py
git commit -m "feat(f8b-trackA): Radio.gatt_subscribe + tests"
```

---

### Task A.4: Unit test for `GattNotification` dataclass

**Files:**
- Modify: `python/tests/test_gatt_notifications.py`

- [ ] **Step 1: Add tests**

Append to `test_gatt_notifications.py`:

```python
def test_gatt_notification_dataclass_fields():
    """GattNotification has handle, value, timestamp."""
    from feralrf.radio import GattNotification

    n = GattNotification(handle=212, value=b"\x01\x02\x03", timestamp=123.456)
    assert n.handle == 212
    assert n.value == b"\x01\x02\x03"
    assert n.timestamp == 123.456


def test_gatt_notification_repr():
    """GattNotification has a useful repr including handle and hex value."""
    from feralrf.radio import GattNotification

    n = GattNotification(handle=0xD4, value=b"\xab\xcd", timestamp=0.0)
    s = repr(n)
    assert "212" in s or "0xd4" in s.lower() or "GattNotification" in s
    assert "abcd" in s.lower() or "ab cd" in s.lower() or "b'\\xab\\xcd'" in s
```

- [ ] **Step 2: Run, expect failure**

```bash
pytest tests/test_gatt_notifications.py::test_gatt_notification_dataclass_fields tests/test_gatt_notifications.py::test_gatt_notification_repr -v
```

Expected: FAIL with `ImportError: cannot import name 'GattNotification' from 'feralrf.radio'`.

---

### Task A.5: Implement `GattNotification` dataclass

**Files:**
- Modify: `python/feralrf/radio.py`

- [ ] **Step 1: Add the dataclass near the top of `radio.py`**

Find the existing `@dataclass` definitions (around `GattService`, `GattCharacteristic`). Add:

```python
@dataclass
class GattNotification:
    """An ATT notification or indication received on a subscribed CCC.

    Async push from firmware: when the peer sends ATT op 0x1B (NOTIFY)
    or 0x1D (INDICATE), the firmware emits RSP_GATT_NOTIFY[handle:2][value:N]
    immediately — no host poll required. The host's read_gatt_notifications()
    iterator yields these as they arrive.
    """

    handle: int
    value: bytes
    timestamp: float  # host monotonic at receive
```

- [ ] **Step 2: Run tests**

```bash
pytest tests/test_gatt_notifications.py::test_gatt_notification_dataclass_fields tests/test_gatt_notifications.py::test_gatt_notification_repr -v
```

Expected: PASS.

- [ ] **Step 3: Commit**

```bash
git add python/feralrf/radio.py python/tests/test_gatt_notifications.py
git commit -m "feat(f8b-trackA): GattNotification dataclass"
```

---

### Task A.6: Unit tests for `read_gatt_notifications` iterator

**Files:**
- Modify: `python/tests/test_gatt_notifications.py`

- [ ] **Step 1: Add tests**

```python
def test_read_gatt_notifications_yields_parsed(radio_mock):
    """An RSP_GATT_NOTIFY frame in the RX buffer yields a GattNotification."""
    from feralrf.radio import GattNotification

    # Frame payload: handle_le[2] + value[N]
    radio_mock._read_response.side_effect = [
        (Response.GATT_NOTIFY, 0, b"\xd4\x00\xab\xcd\xef"),
        TimeoutError("no more frames"),  # signal end-of-stream
    ]

    results = list(radio_mock.read_gatt_notifications(timeout=1.0))

    assert len(results) == 1
    n = results[0]
    assert isinstance(n, GattNotification)
    assert n.handle == 0xD4
    assert n.value == b"\xab\xcd\xef"
    assert n.timestamp > 0


def test_read_gatt_notifications_stops_on_timeout(radio_mock):
    """If no frames arrive, iterator ends cleanly without raising."""
    radio_mock._read_response.side_effect = TimeoutError("no frames")

    results = list(radio_mock.read_gatt_notifications(timeout=0.5))
    assert results == []


def test_read_gatt_notifications_filters_other_frames(radio_mock):
    """Other unsolicited frames (e.g., RSP_DISCONNECTED) are ignored or end stream."""
    radio_mock._read_response.side_effect = [
        (Response.GATT_NOTIFY, 0, b"\xd4\x00\x11"),
        (Response.GATT_NOTIFY, 0, b"\xaa\x00\x22\x33"),
        TimeoutError("end"),
    ]

    results = list(radio_mock.read_gatt_notifications(timeout=2.0))
    assert len(results) == 2
    assert results[0].handle == 0xD4
    assert results[0].value == b"\x11"
    assert results[1].handle == 0xAA
    assert results[1].value == b"\x22\x33"


def test_read_gatt_notifications_handles_short_payload(radio_mock):
    """Malformed payload (< 2 bytes for handle) is skipped, not raised."""
    radio_mock._read_response.side_effect = [
        (Response.GATT_NOTIFY, 0, b"\x01"),  # too short
        (Response.GATT_NOTIFY, 0, b"\xd4\x00"),  # zero-length value (valid)
        TimeoutError("end"),
    ]

    results = list(radio_mock.read_gatt_notifications(timeout=1.0))
    # First frame skipped, second yields handle=0xD4, value=b""
    assert len(results) == 1
    assert results[0].handle == 0xD4
    assert results[0].value == b""
```

- [ ] **Step 2: Run, expect failure**

```bash
pytest tests/test_gatt_notifications.py -v -k "read_gatt"
```

Expected: 4 tests FAIL with `AttributeError: 'Radio' object has no attribute 'read_gatt_notifications'`.

---

### Task A.7: Implement `read_gatt_notifications` iterator

**Files:**
- Modify: `python/feralrf/radio.py`

- [ ] **Step 1: Add the method**

Place it adjacent to `gatt_subscribe`:

```python
def read_gatt_notifications(
    self,
    timeout: float = 5.0,
) -> "Iterator[GattNotification]":
    """Yield GattNotification frames as they arrive.

    Iterates over RX frames from the firmware, filtering for
    RSP_GATT_NOTIFY. Other unsolicited frames (e.g., RSP_DISCONNECTED)
    end the iterator. Iterator ends quietly on timeout — caller can
    loop and call again.

    Args:
        timeout: Seconds to wait for the next frame. The iterator
            ends when this elapses without a new RSP_GATT_NOTIFY.

    Yields:
        GattNotification per received frame.

    Note:
        If the host is slower than the peripheral, the COBS RX buffer
        can overflow and frames are lost. Backpressure is deferred to
        F8c.
    """
    import time
    while True:
        try:
            cmd_id, _seq, payload = self._read_response(
                timeout=timeout,
                expected={Response.GATT_NOTIFY},
            )
        except TimeoutError:
            return
        if cmd_id != Response.GATT_NOTIFY:
            return
        if len(payload) < 2:
            # Malformed — skip
            continue
        handle = int.from_bytes(payload[0:2], "little")
        value = bytes(payload[2:])
        yield GattNotification(
            handle=handle,
            value=value,
            timestamp=time.monotonic(),
        )
```

Add `"read_gatt_notifications"` to `STABLE_METHODS`.

- [ ] **Step 2: Run all tests**

```bash
pytest tests/test_gatt_notifications.py -v
```

Expected: all tests PASS (10 tests total: 4 subscribe + 2 dataclass + 4 iterator).

- [ ] **Step 3: Commit**

```bash
git add python/feralrf/radio.py python/tests/test_gatt_notifications.py
git commit -m "feat(f8b-trackA): Radio.read_gatt_notifications iterator + tests"
```

---

### Task A.8: Firmware — ATT 0x1B handler in att_client.c

**Files:**
- Modify: `firmware/cc1352/src/att_client.c`
- Modify: `firmware/cc1352/include/att_client.h`

- [ ] **Step 1: Locate the ATT incoming dispatch**

```bash
grep -n "case 0x[01]\|ATT_OP\|incoming_opcode\|op = data\[0\]" firmware/cc1352/src/att_client.c | head -20
```

Find the function that switches on incoming ATT opcodes. It will look like a `switch (opcode)` with cases for `0x01` (ERROR_RSP), `0x0B` (READ_RSP), `0x13` (WRITE_RSP).

- [ ] **Step 2: Add the 0x1B handler**

In the switch, add:

```c
case 0x1B: /* ATT_HANDLE_VALUE_NOTIFICATION */
    /* Payload after the opcode: handle[2] + value[N] */
    if (len < 3u) {
        /* Malformed; drop silently. */
        s_metrics.notif_bad++;
        return;
    }
    AttClient_emitNotification(&data[1], (uint8_t)(len - 1u));
    s_metrics.notif_rx++;
    return;
```

Where the helper is added at file scope (above the dispatch function):

```c
static void AttClient_emitNotification(const uint8_t *handle_and_value, uint8_t len) {
    /* Build RSP_GATT_NOTIFY [handle:2][value:N].
     * The firmware framer takes (cmd_id, seq, payload, payload_len).
     * Notifications are unsolicited — use s_active_seq=0 (host ignores seq for streaming responses). */
    send_response(RSP_GATT_NOTIFY, 0u, handle_and_value, len);
}
```

If `s_metrics` doesn't have `notif_rx`/`notif_bad` fields yet, add them — find the metrics struct definition (likely `radio_if.h` or a metrics header) and extend it.

- [ ] **Step 3: Build firmware and verify clean**

```bash
cd firmware/cc1352/build && cmake --build . -j$(nproc)
```

Expected: clean build, no new warnings on `att_client.c`.

- [ ] **Step 4: Commit**

```bash
git add firmware/cc1352/src/att_client.c firmware/cc1352/include/*.h
git commit -m "feat(f8b-trackA): firmware ATT_HANDLE_VALUE_NOTIFICATION (0x1B) handler"
```

---

### Task A.9: Firmware — ATT 0x1D handler with auto-CFM

**Files:**
- Modify: `firmware/cc1352/src/att_client.c`

- [ ] **Step 1: Add the 0x1D handler in the same switch**

```c
case 0x1D: /* ATT_HANDLE_VALUE_INDICATION */
    if (len < 3u) {
        s_metrics.notif_bad++;
        return;
    }
    AttClient_emitNotification(&data[1], (uint8_t)(len - 1u));
    s_metrics.notif_rx++;
    /* Indications require ATT_HANDLE_VALUE_CONFIRMATION (0x1E) back to peer. */
    AttClient_sendCfm();
    return;
```

- [ ] **Step 2: Implement `AttClient_sendCfm`**

```c
static void AttClient_sendCfm(void) {
    /* ATT_HANDLE_VALUE_CONFIRMATION = single byte 0x1E.
     * Send via the L2CAP/ATT TX path — find the existing ATT TX helper
     * (search: AttClient_send, attTx, l2capSend). */
    uint8_t cfm = 0x1E;
    AttClient_sendOnAttChannel(&cfm, 1u);  /* adapt name to existing helper */
}
```

- [ ] **Step 3: Build, flash, and run a quick manual test**

```bash
cd firmware/cc1352/build && cmake --build . -j$(nproc)
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
HEX=/home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
python -m catnip flash -d 1 "$HEX"
```

Manual smoke (no CMD_GATT_SUBSCRIBE wired yet, so no formal test — just verify firmware boots and existing GATT smokes still work):

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
python examples/lab/diag_attclient_repro.py /dev/ttyACM2
```

Expected: still 5/5 cycles per Track 0 fix.

- [ ] **Step 4: Commit**

```bash
git add firmware/cc1352/src/att_client.c
git commit -m "feat(f8b-trackA): firmware ATT_HANDLE_VALUE_INDICATION (0x1D) handler with auto-CFM"
```

---

### Task A.10: Firmware — CMD_GATT_SUBSCRIBE dispatch

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c`

- [ ] **Step 1: Add the dispatch case**

Find the switch in `command_processor.c` (search for `case CMD_GATT_READ:`). Add a new case:

```c
case CMD_GATT_SUBSCRIBE: {
    /* Payload: handle_le[2] + enable[1] + indicate[1] = 4 bytes */
    if (payload_len != 4u) {
        send_error(seq, ERR_INVALID_PAYLOAD);
        return;
    }
    if (!BleConn_isConnected()) {
        send_error(seq, ERR_INVALID_STATE);
        return;
    }
    ensure_gatt_callbacks();
    s_gatt_seq = seq;
    uint16_t handle = read_u16_le(payload);
    bool enable = (payload[2] != 0u);
    bool indicate = (payload[3] != 0u);
    uint16_t ccc_value = enable ? (indicate ? 0x0002u : 0x0001u) : 0x0000u;
    uint8_t ccc_bytes[2] = {(uint8_t)(ccc_value & 0xFFu), (uint8_t)(ccc_value >> 8)};
    /* CCC handle convention: char value handle + 1.
     * If a peripheral lays out descriptors differently, the host must call
     * gatt_write directly to the correct CCC handle. */
    if (!AttClient_startWrite((uint16_t)(handle + 1u), ccc_bytes, 2u)) {
        send_error(seq, ERR_INVALID_STATE);
        return;
    }
    send_ack(seq);
    return;
}
```

- [ ] **Step 2: Build and flash**

```bash
cd firmware/cc1352/build && cmake --build . -j$(nproc)
cd /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip
HEX=/home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build/feralrf_cc1352.hex
python -m catnip flash -d 1 "$HEX"
```

- [ ] **Step 3: Manual end-to-end probe**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
python -c "
import time
from feralrf import Radio
r = Radio('/dev/ttyACM2'); r.connect(); time.sleep(0.3); r.init()
addr_le = bytes.fromhex('f87d8ae8e6a8')
r.ble_connect(addr_le, 0)
r.gatt_discover()
r.gatt_subscribe(handle=212)
print('subscribe ACK ok')
r.ble_disconnect()
r.disconnect()
"
```

Expected: prints `subscribe ACK ok`, no exceptions.

- [ ] **Step 4: Commit**

```bash
git add firmware/cc1352/src/command_processor.c
git commit -m "feat(f8b-trackA): firmware CMD_GATT_SUBSCRIBE dispatch"
```

---

### Task A.11: Hardware smoke test against Sony WH-CH720N

**Files:**
- Create: `python/examples/lab/smoke_f8b_notifications.py`

- [ ] **Step 1: Write the smoke**

```python
#!/usr/bin/env python3
"""F8b Track A — wire-level smoke for GATT notifications.

Connects to Sony WH-CH720N, discovers, subscribes to a panel of
custom Sony characteristics, and waits 30s for the user to press
buttons (NC toggle, play/pause, etc.). Closure: at least one
notification captured on any subscribed handle.

Usage: python smoke_f8b_notifications.py [port] [target_mac]
"""
import argparse
import sys
import time

from feralrf import Radio

# Sony custom service notification handles per
# docs/investigations/2026-05-01-sony-wh-ch720n.md
SONY_NOTIFY_HANDLES = (170, 186, 194, 212, 564, 580, 612)


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="/dev/ttyACM2")
    p.add_argument("--target-mac", default="A8:E6:E8:8A:7D:F8")
    p.add_argument("--duration", type=float, default=30.0)
    args = p.parse_args()

    addr_le = bytes.fromhex("".join(args.target_mac.split(":")[::-1]))
    r = Radio(args.port)
    r.connect(); time.sleep(0.3); r.init()

    print(f"[STEP] connect {args.target_mac}")
    res = r.ble_connect(addr_le, addr_type=0, timeout=10.0)
    if res.result != 0:
        print(f"  CONNECT FAILED code={res.result}")
        return 1
    time.sleep(0.5)

    print("[STEP] gatt_discover")
    disc = r.gatt_discover(timeout=15.0)
    print(f"  services={len(disc.services)} chars={len(disc.characteristics)}")

    print(f"[STEP] subscribe to {len(SONY_NOTIFY_HANDLES)} candidate handles")
    subscribed = []
    for h in SONY_NOTIFY_HANDLES:
        try:
            r.gatt_subscribe(handle=h, enable=True)
            subscribed.append(h)
            print(f"  h{h:>3}  OK")
        except Exception as e:
            print(f"  h{h:>3}  FAIL ({type(e).__name__})")

    if not subscribed:
        print("[FAIL] no handle subscribable")
        r.ble_disconnect(); r.disconnect()
        return 1

    print(f"[STEP] waiting {args.duration:.0f}s — press buttons on the headphones now")
    t0 = time.time()
    notifs: list = []
    while time.time() - t0 < args.duration:
        for n in r.read_gatt_notifications(timeout=1.0):
            notifs.append(n)
            print(f"  [{time.time()-t0:5.1f}s] h{n.handle}: {n.value.hex()}")

    r.ble_disconnect(); r.disconnect()

    print()
    if notifs:
        print(f"[ OK ] F8b Track A smoke PASS — captured {len(notifs)} notifications")
        return 0
    print("[FAIL] no notifications captured — try pressing more buttons or check that the headphones are awake")
    return 1


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Run the smoke (user interactive)**

User instructions: keep WH-CH720N powered on, not connected to phone. Run:

```bash
cd python && source .venv/bin/activate
python examples/lab/smoke_f8b_notifications.py
```

Press: NC button → ambient mode → NC off → play/pause → volume buttons during the 30s window.

Expected: at least 1 notification printed; final line `[ OK ] F8b Track A smoke PASS`.

- [ ] **Step 3: Commit smoke**

```bash
git add python/examples/lab/smoke_f8b_notifications.py
git commit -m "test(f8b-trackA): wire-level smoke for notifications against WH-CH720N"
```

---

### Task A.12: Pre-commit + acceptance gate verification

- [ ] **Step 1: Run pre-commit on all touched files**

```bash
pre-commit run --files \
    firmware/cc1352/src/att_client.c \
    firmware/cc1352/src/command_processor.c \
    firmware/cc1352/include/att_client.h \
    python/feralrf/radio.py \
    python/feralrf/commands.py \
    python/feralrf/responses.py \
    python/tests/test_gatt_notifications.py \
    python/examples/lab/smoke_f8b_notifications.py \
    python/examples/lab/diag_attclient_repro.py
```

Expected: all checks pass. If clang-format reformats the firmware, re-run and verify clean. Stage and amend the last commit if the touched files include uncommitted formatting.

- [ ] **Step 2: Full unit test suite**

```bash
cd python && pytest -m "not hardware"
```

Expected: all tests pass (existing 322 + 10 new from Track A).

- [ ] **Step 3: Repro 3x of the AttClient bug fix gate**

```bash
for i in 1 2 3; do
    echo "--- run $i ---"
    python examples/lab/diag_attclient_repro.py /dev/ttyACM2
done
```

Expected: each run reports 5/5 cycles successful.

- [ ] **Step 4: Repro 3x of the Track A smoke**

```bash
for i in 1 2 3; do
    echo "--- smoke run $i ---"
    python examples/lab/smoke_f8b_notifications.py --duration 20
done
```

Expected: 3/3 PASS.

- [ ] **Step 5: Update memory**

Add a new memory file summarising Track A landed:

```bash
cat > /home/sabas/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f8b_track_a.md << 'MEMEOF'
---
name: F8b Track A — GATT notifications + AttClient bug
description: Track A of F8b done <DATE>: ATT 0x1B/0x1D notification reception, host gatt_subscribe + read_gatt_notifications, AttClient stale-state bug fixed (or deferred — fill in).
type: project
---
Track A of F8b landed <DATE> on branch feature/ti-rtos-migration.
- Firmware att_client.c handles ATT_HANDLE_VALUE_NOTIFICATION (0x1B) and INDICATION (0x1D, with auto-CFM).
- New CMD_GATT_SUBSCRIBE (0x46) writes the CCC at handle+1.
- Host API: r.gatt_subscribe(handle), r.read_gatt_notifications(timeout) iterator, GattNotification dataclass.
- AttClient stale-state bug: <"FIXED at <SHA>" | "DEFERRED to F8c — root cause is X, attempted fix is Y">.
- Smoke: WH-CH720N notifications captured 3/3 runs, 30s each.
- 10 new unit tests, all passing.

Outstanding for F8b: Track B (Sniffle observer) and Track C (Pairing JW).
MEMEOF
```

Add to MEMORY.md index.

- [ ] **Step 6: Final commit + summary**

```bash
git add -A
git commit -m "docs(f8b-trackA): close Track A — notifications + AttClient bug

All Track A acceptance gates pass:
- Unit tests 332+ green
- AttClient bug repro 3/3 cycles all pass without reset_device
- WH-CH720N notifications smoke 3/3
- Pre-commit clean

Track B (Sniffle observer) and Track C (Pairing JW) remain.
"
```

---

## Done bar for Track A

- All firmware files clean clang-format + cppcheck.
- All Python files clean black + isort + flake8 + mypy.
- 10+ new unit tests, all green.
- AttClient repro 3 runs × 5 cycles = 15/15 successes (or explicit deferral memo).
- WH-CH720N notification smoke 3/3 with ≥1 notification captured per run.
- Memory updated.
- All commits on branch `feature/ti-rtos-migration` (or feature branch FF'd in).

---

## What this plan does NOT cover

- **Track B (Sniffle-style passive connection follower)** — separate plan
  `2026-05-XX-f8b-track-b-follower.md` to write after Track A lands.
- **Track C (Pairing Just Works)** — separate plan
  `2026-05-XX-f8b-track-c-pairing.md` to write after Track B lands.
- **Final F8b acceptance + tag `v2.0-f8b`** — happens after C; will be a
  short closure plan with smoke_f8b_full.py + tag.
