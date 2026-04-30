# F22 Test modes CW + PRBS — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose CC1352P7 `rfc_CMD_TX_TEST` via 3 commands so users can emit a CW carrier or PRBS-modulated continuous test signal on the currently-selected PHY.

**Architecture:** Pure additive — 3 new firmware command ids (`0x55`/`0x56`/`0x57`), 3 Python `Radio` methods, 2 firmware functions in `radio_if.c`. Reuses existing PHY/channel state set by `set_phy(...)` (no new PHY enum). Reuses existing `s_last_tx_status` telemetry exposed via `debug_timing`.

**Tech Stack:** Python 3.10+, TI SimpleLink CC13xx/CC26xx SDK 8.30, TI-RTOS7. No new deps.

**Spec:** `docs/superpowers/specs/2026-04-29-f22-test-modes-design.md`

**Branch:** `feature/f22-test-modes` (forked from `feature/ti-rtos-migration` HEAD `f8d7921`).

---

## File Structure

Files to create:

| File | Responsibility | LOC |
|---|---|---|
| `python/tests/test_tx_test.py` | 7 unit tests for command ids, payloads, validation | ~50 |
| `python/examples/lab/smoke_f22_tx_test.py` | hardware smoke (5 tests on 2 boards) | ~110 |

Files to modify:

| File | Change | LOC |
|---|---|---|
| `python/feralrf/enums.py` | 3 enum entries + STABLE_COMMANDS additions | +5 |
| `python/feralrf/radio.py` | 3 methods (`tx_cw`, `tx_prbs`, `tx_test_stop`) | +50 |
| `firmware/cc1352/include/radio_if.h` | 2 function declarations | +2 |
| `firmware/cc1352/src/radio_if.c` | state + 2 functions | +60 |
| `firmware/cc1352/src/command_processor.c` | 3 defines + 3 case handlers | +30 |

Total: ~310 LOC. Zero new files in firmware. Zero protocol changes.

---

## Task 1: Add Command enum + register in STABLE_COMMANDS

**Files:**
- Modify: `python/feralrf/enums.py`
- Test: `python/tests/test_tx_test.py` (new)

- [ ] **Step 1: Write the failing tests**

Create `python/tests/test_tx_test.py`:

```python
"""Unit tests for F22 test mode commands (CW + PRBS).

Hardware-free contract tests: command IDs, payload builders, and
the error path for invalid PRBS pattern. Hardware end-to-end coverage
lives in python/examples/lab/smoke_f22_tx_test.py.
"""

import pytest

from feralrf.enums import STABLE_COMMANDS, Command


def test_tx_cw_command_id():
    assert Command.TX_CW == 0x55


def test_tx_prbs_command_id():
    assert Command.TX_PRBS == 0x56


def test_tx_test_stop_command_id():
    assert Command.TX_TEST_STOP == 0x57


def test_tx_test_commands_in_stable():
    assert Command.TX_CW in STABLE_COMMANDS
    assert Command.TX_PRBS in STABLE_COMMANDS
    assert Command.TX_TEST_STOP in STABLE_COMMANDS
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
.venv/bin/python -m pytest tests/test_tx_test.py -v
```

Expected: 4 FAIL with `AttributeError: TX_CW` or similar.

- [ ] **Step 3: Add the enum entries**

In `python/feralrf/enums.py`, after the existing `TX_STOP = 0x24` line (around line 52), insert:

```python
    TX_CW = 0x55
    TX_PRBS = 0x56
    TX_TEST_STOP = 0x57
```

Then in the `STABLE_COMMANDS` tuple (around line 97), append after `Command.TX_STOP,`:

```python
    Command.TX_CW,
    Command.TX_PRBS,
    Command.TX_TEST_STOP,
```

- [ ] **Step 4: Run test to verify it passes**

```bash
.venv/bin/python -m pytest tests/test_tx_test.py -v
```

Expected: 4 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/enums.py python/tests/test_tx_test.py
git commit -m "feat(f22): add TX_CW/TX_PRBS/TX_TEST_STOP command enum entries"
```

---

## Task 2: Python `tx_cw` method

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_tx_test.py`

- [ ] **Step 1: Append the failing test**

Append to `python/tests/test_tx_test.py`:

```python
def test_tx_cw_sends_correct_frame(monkeypatch):
    """tx_cw issues SET_POWER then a TX_CW frame with empty payload."""
    from feralrf import Radio
    from feralrf.enums import Command, Response
    from feralrf.protocol import build_frame

    radio = Radio(port="dummy")
    sent_cmds = []

    def fake_send_command(cmd, payload=b""):
        sent_cmds.append((cmd, bytes(payload)))

    def fake_read_response(timeout=1.0, expected=None):
        return (Response.ACK, 0, b"")

    monkeypatch.setattr(radio, "_send_command", fake_send_command)
    monkeypatch.setattr(radio, "_read_response", fake_read_response)

    radio.tx_cw(power_dbm=5)

    cmd_ids = [c[0] for c in sent_cmds]
    assert Command.SET_POWER in cmd_ids
    assert Command.TX_CW in cmd_ids
    # TX_CW frame has no payload
    cw_frame = next(c for c in sent_cmds if c[0] == Command.TX_CW)
    assert cw_frame[1] == b""
```

- [ ] **Step 2: Run test to verify it fails**

```bash
.venv/bin/python -m pytest tests/test_tx_test.py::test_tx_cw_sends_correct_frame -v
```

Expected: FAIL with `AttributeError: 'Radio' object has no attribute 'tx_cw'`.

- [ ] **Step 3: Add the method**

In `python/feralrf/radio.py`, find the `transmit_continuous` method (around line 1050) and add **after** `stop_transmit` (around line 1072) the following method:

```python
    def tx_cw(self, power_dbm: int = 0) -> None:
        """Emit unmodulated carrier on current PHY/channel.

        Requires set_phy(...) first to select band + channel/frequency.
        Stop with tx_test_stop(). Test signal runs until cancelled.

        Args:
            power_dbm: TX power, -20 to +5 dBm (std-PA cap on this hw rev).

        Raises:
            CommandError: if no PHY is set or RF Core rejects the command.
        """
        self.set_power(power_dbm)
        self._send_command(Command.TX_CW)
        cmd_id, _seq, payload = self._read_response(
            expected={Response.ACK, Response.ERROR}
        )
        if cmd_id == Response.ERROR:
            raise CommandError("tx_cw failed", payload[0] if payload else 0)
```

- [ ] **Step 4: Register the method in STABLE_METHODS**

Around line 165 of `python/feralrf/radio.py` (find the methods list), insert `"tx_cw",` alphabetically near the existing `transmit_*` entries.

- [ ] **Step 5: Run test to verify it passes**

```bash
.venv/bin/python -m pytest tests/test_tx_test.py -v
```

Expected: 5 PASSED.

- [ ] **Step 6: Commit**

```bash
git add python/feralrf/radio.py python/tests/test_tx_test.py
git commit -m "feat(f22): Radio.tx_cw() Python wrapper for CMD_TX_CW"
```

---

## Task 3: Python `tx_prbs` method + ValueError on bad pattern

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_tx_test.py`

- [ ] **Step 1: Append failing tests**

Append to `python/tests/test_tx_test.py`:

```python
def test_tx_prbs_pattern_prbs9(monkeypatch):
    """tx_prbs(pattern='prbs9') sends payload byte 0x01."""
    from feralrf import Radio
    from feralrf.enums import Command, Response

    radio = Radio(port="dummy")
    sent_cmds = []

    monkeypatch.setattr(radio, "_send_command",
                         lambda c, p=b"": sent_cmds.append((c, bytes(p))))
    monkeypatch.setattr(radio, "_read_response",
                         lambda timeout=1.0, expected=None: (Response.ACK, 0, b""))

    radio.tx_prbs(power_dbm=0, pattern="prbs9")

    prbs_frame = next(c for c in sent_cmds if c[0] == Command.TX_PRBS)
    assert prbs_frame[1] == bytes([0x01])


def test_tx_prbs_pattern_prbs15(monkeypatch):
    """tx_prbs(pattern='prbs15') sends payload byte 0x02."""
    from feralrf import Radio
    from feralrf.enums import Command, Response

    radio = Radio(port="dummy")
    sent_cmds = []
    monkeypatch.setattr(radio, "_send_command",
                         lambda c, p=b"": sent_cmds.append((c, bytes(p))))
    monkeypatch.setattr(radio, "_read_response",
                         lambda timeout=1.0, expected=None: (Response.ACK, 0, b""))

    radio.tx_prbs(power_dbm=0, pattern="prbs15")

    prbs_frame = next(c for c in sent_cmds if c[0] == Command.TX_PRBS)
    assert prbs_frame[1] == bytes([0x02])


def test_tx_prbs_invalid_pattern_raises():
    """Unknown pattern strings raise ValueError before any IO."""
    from feralrf import Radio

    radio = Radio(port="dummy")
    with pytest.raises(ValueError, match="prbs9.*prbs15"):
        radio.tx_prbs(power_dbm=0, pattern="prbs99")
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_tx_test.py -v
```

Expected: 3 FAIL with `AttributeError: 'Radio' object has no attribute 'tx_prbs'`.

- [ ] **Step 3: Add the method**

In `python/feralrf/radio.py`, **after** `tx_cw` (added in Task 2), insert:

```python
    def tx_prbs(self, power_dbm: int = 0, pattern: str = "prbs9") -> None:
        """Emit PRBS-modulated test signal on current PHY/channel.

        Args:
            power_dbm: TX power, -20 to +5 dBm.
            pattern: 'prbs9' (default, BLE DTM compliant) or 'prbs15'.

        Raises:
            ValueError: if pattern not 'prbs9' or 'prbs15'.
            CommandError: if no PHY is set or RF Core rejects the command.
        """
        mode_byte = {"prbs9": 0x01, "prbs15": 0x02}.get(pattern.lower())
        if mode_byte is None:
            raise ValueError(
                f"pattern must be 'prbs9' or 'prbs15', got {pattern!r}"
            )
        self.set_power(power_dbm)
        self._send_command(Command.TX_PRBS, bytes([mode_byte]))
        cmd_id, _seq, payload = self._read_response(
            expected={Response.ACK, Response.ERROR}
        )
        if cmd_id == Response.ERROR:
            raise CommandError("tx_prbs failed", payload[0] if payload else 0)
```

- [ ] **Step 4: Register in STABLE_METHODS**

In the same methods list as Task 2 step 4 (around line 165), insert `"tx_prbs",` after `"tx_cw",`.

- [ ] **Step 5: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_tx_test.py -v
```

Expected: 8 PASSED total (4 + 1 + 3).

- [ ] **Step 6: Commit**

```bash
git add python/feralrf/radio.py python/tests/test_tx_test.py
git commit -m "feat(f22): Radio.tx_prbs() Python wrapper with prbs9/prbs15 + ValueError"
```

---

## Task 4: Python `tx_test_stop` method

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_tx_test.py`

- [ ] **Step 1: Append failing test**

Append to `python/tests/test_tx_test.py`:

```python
def test_tx_test_stop_no_payload(monkeypatch):
    """tx_test_stop sends TX_TEST_STOP with empty payload."""
    from feralrf import Radio
    from feralrf.enums import Command, Response

    radio = Radio(port="dummy")
    sent_cmds = []
    monkeypatch.setattr(radio, "_send_command",
                         lambda c, p=b"": sent_cmds.append((c, bytes(p))))
    monkeypatch.setattr(radio, "_read_response",
                         lambda timeout=1.0, expected=None: (Response.ACK, 0, b""))

    radio.tx_test_stop()

    stop_frame = next(c for c in sent_cmds if c[0] == Command.TX_TEST_STOP)
    assert stop_frame[1] == b""
```

- [ ] **Step 2: Run test to verify it fails**

```bash
.venv/bin/python -m pytest tests/test_tx_test.py -v
```

Expected: FAIL with `AttributeError: 'Radio' object has no attribute 'tx_test_stop'`.

- [ ] **Step 3: Add the method**

In `python/feralrf/radio.py`, **after** `tx_prbs` (added in Task 3), insert:

```python
    def tx_test_stop(self) -> None:
        """Stop any active CW or PRBS test signal. Idempotent — safe to call
        when no test is running.

        Raises:
            CommandError: only if firmware reports an unexpected error.
        """
        self._send_command(Command.TX_TEST_STOP)
        cmd_id, _seq, payload = self._read_response(
            expected={Response.ACK, Response.ERROR}
        )
        if cmd_id == Response.ERROR:
            raise CommandError(
                "tx_test_stop failed", payload[0] if payload else 0
            )
```

- [ ] **Step 4: Register in STABLE_METHODS**

In the methods list, insert `"tx_test_stop",` after `"tx_prbs",`.

- [ ] **Step 5: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_tx_test.py -v
```

Expected: 9 PASSED total.

- [ ] **Step 6: Run pre-commit on Python changes**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/feralrf/enums.py python/feralrf/radio.py python/tests/test_tx_test.py
```

Expected: all hooks pass (black/isort may reformat — re-stage and re-run if so).

- [ ] **Step 7: Commit**

```bash
git add python/feralrf/radio.py python/tests/test_tx_test.py
git commit -m "feat(f22): Radio.tx_test_stop() Python wrapper, idempotent"
```

---

## Task 5: Firmware command-processor handlers (stubs that ACK)

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c`

- [ ] **Step 1: Add command-id defines**

In `firmware/cc1352/src/command_processor.c`, after `#define CMD_TX_STOP 0x24u` (around line 34), insert:

```c
#define CMD_TX_CW 0x55u
#define CMD_TX_PRBS 0x56u
#define CMD_TX_TEST_STOP 0x57u
```

- [ ] **Step 2: Add error code define if not present**

Search the file for `ERR_RF_NOT_READY`. If absent, add near the other ERR defines:

```c
#define ERR_RF_NOT_READY 0x07u
#define ERR_TX_FAILED 0x08u
```

(If these already exist, skip this step.)

- [ ] **Step 3: Add case handlers**

In `handle_command` (around the TX_STOP case at line 417), insert these cases after `case CMD_TX_STOP:`:

```c
    case CMD_TX_CW:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!RadioIF_runTxTest(0u)) {
            send_error(seq, ERR_RF_NOT_READY);
            return;
        }
        send_ack(seq);
        return;

    case CMD_TX_PRBS:
        if (payload_len != 1u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (payload[0] != 1u && payload[0] != 2u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (!RadioIF_runTxTest(payload[0])) {
            send_error(seq, ERR_RF_NOT_READY);
            return;
        }
        send_ack(seq);
        return;

    case CMD_TX_TEST_STOP:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        RadioIF_stopTxTest();
        send_ack(seq);
        return;
```

- [ ] **Step 4: Build firmware (will fail — RadioIF_runTxTest not yet declared)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
cmake --build . -j$(nproc) 2>&1 | grep "error:" | head -3
```

Expected: errors about `implicit declaration of function 'RadioIF_runTxTest'` — that's fine; will be resolved in Task 6.

- [ ] **Step 5: Commit (compile-incomplete — declarations come next)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git add firmware/cc1352/src/command_processor.c
git commit -m "feat(f22): command_processor handlers for TX_CW/TX_PRBS/TX_TEST_STOP (stubs)"
```

---

## Task 6: Firmware function declarations

**Files:**
- Modify: `firmware/cc1352/include/radio_if.h`

- [ ] **Step 1: Add declarations**

In `firmware/cc1352/include/radio_if.h`, find the existing `RadioIF_transmitRaw` declaration (around line 51) and after it, insert:

```c
/* F22 test modes — CW (mode=0) or PRBS-9 (mode=1) / PRBS-15 (mode=2).
 * Requires a prior set_phy() so s_rf_handle is open. Returns false if not. */
bool RadioIF_runTxTest(uint8_t mode);
void RadioIF_stopTxTest(void);
```

- [ ] **Step 2: Build firmware (will fail — implementations not yet present)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
cmake --build . -j$(nproc) 2>&1 | grep -E "error:|undefined reference" | head -3
```

Expected: linker errors `undefined reference to 'RadioIF_runTxTest'` and `'RadioIF_stopTxTest'`. That's fine; resolved in Task 7-8.

- [ ] **Step 3: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git add firmware/cc1352/include/radio_if.h
git commit -m "feat(f22): declare RadioIF_runTxTest / RadioIF_stopTxTest in radio_if.h"
```

---

## Task 7: Firmware `RadioIF_runTxTest` implementation

**Files:**
- Modify: `firmware/cc1352/src/radio_if.c`

- [ ] **Step 1: Add static state**

In `firmware/cc1352/src/radio_if.c`, find the `s_non433_handle` declaration added by F9 (around line 235). After it, insert:

```c
/* F22 test mode — CW or PRBS via rfc_CMD_TX_TEST. Single in-flight cmd. */
static rfc_CMD_TX_TEST_t s_cmd_tx_test;
static RF_CmdHandle s_test_cmd_handle = RF_SCHEDULE_CMD_ERROR;
```

- [ ] **Step 2: Add the function**

Append to `firmware/cc1352/src/radio_if.c` (just before the closing of the file or after `RadioIF_init`):

```c
bool RadioIF_runTxTest(uint8_t mode) {
    if (s_rf_handle == NULL) {
        return false;
    }
    /* Idempotent: if a previous test is running, stop it first. */
    if (s_test_cmd_handle >= 0) {
        RadioIF_stopTxTest();
    }

    memset(&s_cmd_tx_test, 0, sizeof(s_cmd_tx_test));
    s_cmd_tx_test.commandNo = CMD_TX_TEST;
    s_cmd_tx_test.config.bUsePrbs9 = (mode == 1u) ? 1u : 0u;
    s_cmd_tx_test.config.bUsePrbs15 = (mode == 2u) ? 1u : 0u;
    s_cmd_tx_test.config.bFsOff = 0u; /* keep FS on */
    s_cmd_tx_test.startTrigger.triggerType = TRIG_NOW;
    s_cmd_tx_test.startTrigger.pastTrig = 1u;
    s_cmd_tx_test.endTrigger.triggerType = TRIG_NEVER; /* runs until cancelled */
    s_cmd_tx_test.endTime = 0u;
    s_cmd_tx_test.condition.rule = COND_NEVER;
    s_cmd_tx_test.status = 0x0000u;

    /* Apply currently-set TX power on the active handle. */
    RadioIF_applyRfTxPower(s_rf_handle,
                           RadioIF_resolveTxPowerValue(s_tx_power_dbm));

    /* Re-tune via FS for current band so the synth is locked at the
     * channel the user picked via set_phy(). */
    if (PhyManager_isBlePhy(s_selected_phy)) {
        Ble5_0_cmdFs.status = 0x0000u;
        RF_postCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdFs,
                   RF_PriorityNormal, NULL, 0);
    } else if (RadioIF_isIeee154PhySelected()) {
        Ieee154_0_cmdFs.status = 0x0000u;
        RF_postCmd(s_rf_handle, (RF_Op *)&Ieee154_0_cmdFs,
                   RF_PriorityNormal, NULL, 0);
    } else if (RadioIF_isSub1ghzPhySelected()) {
        RF_Op *fs = (s_current_rf_mode == &Prop0_mode433)
                        ? (RF_Op *)&Prop0_cmdFs433
                        : (RF_Op *)&Prop0_cmdFs;
        fs->status = 0x0000u;
        RF_postCmd(s_rf_handle, fs, RF_PriorityNormal, NULL, 0);
    }

    s_test_cmd_handle = RF_postCmd(s_rf_handle, (RF_Op *)&s_cmd_tx_test,
                                   RF_PriorityNormal, NULL, 0);
    s_last_tx_status = s_cmd_tx_test.status;
    return s_test_cmd_handle >= 0;
}
```

- [ ] **Step 3: Build firmware (will still fail — stopTxTest missing)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
cmake --build . -j$(nproc) 2>&1 | grep -E "error:|undefined reference" | head -3
```

Expected: only one undefined reference left: `RadioIF_stopTxTest`. Resolved in Task 8.

- [ ] **Step 4: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git add firmware/cc1352/src/radio_if.c
git commit -m "feat(f22): RadioIF_runTxTest — CMD_TX_TEST with CW/PRBS9/PRBS15 config"
```

---

## Task 8: Firmware `RadioIF_stopTxTest` implementation + build

**Files:**
- Modify: `firmware/cc1352/src/radio_if.c`

- [ ] **Step 1: Add the function**

In `firmware/cc1352/src/radio_if.c`, immediately after the `RadioIF_runTxTest` function added in Task 7, insert:

```c
void RadioIF_stopTxTest(void) {
    if (s_rf_handle != NULL && s_test_cmd_handle >= 0) {
        RF_cancelCmd(s_rf_handle, s_test_cmd_handle, 0);
        RF_flushCmd(s_rf_handle, RF_CMDHANDLE_FLUSH_ALL, 0);
    }
    s_test_cmd_handle = RF_SCHEDULE_CMD_ERROR;
}
```

- [ ] **Step 2: Build firmware (should now succeed)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/firmware/cc1352/build
cmake --build . -j$(nproc) 2>&1 | tail -5
```

Expected: `[100%] Built target feralrf_cc1352.elf` with no errors. Some pre-existing warnings (unused statics) are unrelated.

- [ ] **Step 3: Commit**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git add firmware/cc1352/src/radio_if.c
git commit -m "feat(f22): RadioIF_stopTxTest — cancel + flush, idempotent"
```

---

## Task 9: Flash both boards + sanity check

**Files:** none (deployment + smoke)

- [ ] **Step 1: Flash board #1 (RX side, /dev/ttyACM8)**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
FW=firmware/cc1352/build/feralrf_cc1352.hex
timeout 60 python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py flash -d 1 $FW 2>&1 | tail -3
```

Expected: `✓ Device restart complete. Firmware is ready to use!`

- [ ] **Step 2: Flash board #2 (TX side, /dev/ttyACM5)**

```bash
timeout 60 python3 /home/sabas/Documents/electroniccats/CatSniffer-Tools/catnip/catnip.py flash -d 2 $FW 2>&1 | tail -3
```

Expected: same restart message.

- [ ] **Step 3: Sanity check — basic BLE RX still works on board #1 (no F22 regression)**

```bash
cd python && timeout 8 .venv/bin/python -c "
import time, warnings; warnings.simplefilter('ignore')
from feralrf import Radio, PHY
r = Radio('/dev/ttyACM8'); r.connect(); time.sleep(0.3); r.init()
r.set_phy(PHY.BLE_1M, channel=37); r.start_rx(); time.sleep(2.0)
pkts = list(r.read_packets(timeout=0.5))
print(f'BLE 1M sanity: {len(pkts)} pkts (expected >30)')
r.disconnect()
"
```

Expected: ≥30 pkts. If 0, firmware is broken — investigate before continuing.

- [ ] **Step 4: Sanity check — Python suite still passes**

```bash
.venv/bin/python -m pytest 2>&1 | tail -3
```

Expected: 322 + 9 (T1-T4) = 331 passed, 1 pre-existing fail (test_read_response_ignores_echoed_command_frames), 5 skipped.

- [ ] **Step 5: No commit needed (deployment only)**

---

## Task 10: Hardware smoke harness + run validation

**Files:**
- Create: `python/examples/lab/smoke_f22_tx_test.py`

- [ ] **Step 1: Write the smoke**

Create `python/examples/lab/smoke_f22_tx_test.py`:

```python
#!/usr/bin/env python3
"""F22 wire-level smoke — CW + PRBS test modes on 2 boards.

Verifies:
  1. CW on Sub-1GHz 868 returns OK status (firmware tx_status telemetry).
  2. CW on BLE 1M ch37 returns OK status (verifies BLE-band code path).
  3. PRBS-9 on Sub-1GHz 868 increases RX scan count vs idle baseline.
  4. PRBS-15 on Sub-1GHz 868 increases RX scan count vs idle baseline.
  5. tx_test_stop is idempotent — safe to call when nothing is running.

Hardware: TX=/dev/ttyACM5 (board #2), RX=/dev/ttyACM8 (board #1).
"""

import argparse
import re
import sys
import time
import warnings

import serial

from feralrf import PHY, Radio

warnings.simplefilter("ignore")

# CMD_TX_TEST status: low byte 0x0X = ok states; 0x80+ = error.
ERROR_STATUS_MIN = 0x0801


def reset(port):
    m = re.search(r"(\d+)$", port)
    if not m:
        return
    shell = port[: m.start(1)] + str(int(m.group(1)) + 2)
    try:
        s = serial.Serial(shell, 115200, timeout=1.0)
        s.write(b"boot\r\n")
        time.sleep(0.5)
        s.write(b"exit\r\n")
        time.sleep(0.3)
        s.close()
    except Exception:
        pass
    time.sleep(3.5)


def cw_check(tx, label, phy, channel=0):
    tx.set_phy(phy, channel=channel)
    tx.tx_cw(power_dbm=5)
    time.sleep(0.5)
    dbg = tx.debug_timing(timeout=1.5)
    tx.tx_test_stop()
    ok = dbg.tx_status < ERROR_STATUS_MIN
    print(
        f"  CW {label:<14} tx_status=0x{dbg.tx_status:04X} "
        f"{'PASS' if ok else 'FAIL'}"
    )
    return ok


def prbs_check(tx, rx, label, pattern, phy, channel=0):
    rx.set_phy(phy, channel=channel)
    rx.start_rx()
    time.sleep(1.0)
    n_idle = sum(1 for _ in rx.read_packets(timeout=0.5))
    rx.stop_rx()

    tx.set_phy(phy, channel=channel)
    tx.tx_prbs(power_dbm=5, pattern=pattern)
    rx.start_rx()
    time.sleep(1.0)
    n_prbs = sum(1 for _ in rx.read_packets(timeout=0.5))
    rx.stop_rx()
    tx.tx_test_stop()

    delta = n_prbs - n_idle
    ok = delta >= 5
    print(
        f"  PRBS {label:<10} idle={n_idle:>2} prbs={n_prbs:>3} "
        f"delta={delta:+d} {'PASS' if ok else 'FAIL'}"
    )
    return ok


def stop_idempotent(tx):
    try:
        tx.tx_test_stop()
        tx.tx_test_stop()  # second call should not raise
        print("  tx_test_stop idempotent: PASS")
        return True
    except Exception as e:
        print(f"  tx_test_stop idempotent: FAIL ({e})")
        return False


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tx-port", default="/dev/ttyACM5")
    ap.add_argument("--rx-port", default="/dev/ttyACM8")
    args = ap.parse_args()

    print("[STEP] reset both")
    reset(args.tx_port)
    reset(args.rx_port)

    tx = Radio(args.tx_port)
    rx = Radio(args.rx_port)
    results = {}
    try:
        tx.connect()
        time.sleep(0.3)
        tx.init()
        rx.connect()
        time.sleep(0.3)
        rx.init()

        print("[STEP] tests")
        results["cw_sub1g"] = cw_check(tx, "Sub1G_868", PHY.SUB_1GHZ_868)
        results["cw_ble"] = cw_check(tx, "BLE_1M ch37", PHY.BLE_1M, channel=37)
        results["prbs9"] = prbs_check(
            tx, rx, "Sub1G_868", "prbs9", PHY.SUB_1GHZ_868
        )
        results["prbs15"] = prbs_check(
            tx, rx, "Sub1G_868", "prbs15", PHY.SUB_1GHZ_868
        )
        results["idempotent"] = stop_idempotent(tx)

        all_ok = all(results.values())
        print()
        print(
            f"[ {'OK' if all_ok else 'FAIL'} ] F22 smoke: "
            f"{sum(results.values())}/5 PASS"
        )
        return 0 if all_ok else 1
    finally:
        try:
            tx.disconnect()
        except Exception:
            pass
        try:
            rx.disconnect()
        except Exception:
            pass


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Run the smoke**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
timeout 90 .venv/bin/python examples/lab/smoke_f22_tx_test.py 2>&1 | tail -10
```

Expected: `[ OK ] F22 smoke: 5/5 PASS` with each individual line PASS.

If any FAIL: investigate before committing. Common causes:
- CW status non-zero: firmware path issue — re-check Task 7 FS post and config bits.
- PRBS delta < 5: check TX power (must be ≥ 0 dBm) and that lab Sub-1GHz is quiet (no other 868 MHz emitters).
- `idempotent` FAIL: stopTxTest not idempotent — re-check Task 8.

- [ ] **Step 3: Pre-commit on smoke**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
pre-commit run --files python/examples/lab/smoke_f22_tx_test.py
```

Expected: black/isort/flake8 may reformat — re-stage and re-run if so.

- [ ] **Step 4: Commit**

```bash
git add python/examples/lab/smoke_f22_tx_test.py
git commit -m "test(f22): hardware smoke validates CW (status) + PRBS (rx delta) + idempotent"
```

---

## Task 11: Closure gate — pre-commit, suite, memory, tag prep

**Files:** none (validation + memory)

- [ ] **Step 1: Pre-commit on all changed files**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git diff --name-only feature/ti-rtos-migration..HEAD | xargs pre-commit run --files 2>&1 | tail -10
```

Expected: all hooks PASS.

- [ ] **Step 2: Full Python test suite (no regression)**

```bash
cd python && .venv/bin/python -m pytest 2>&1 | tail -3
```

Expected: 322 + 9 = 331 passed, 1 pre-existing fail, 5 skipped.

- [ ] **Step 3: Re-run F9 stress (no regression on PHY switching)**

```bash
timeout 90 .venv/bin/python examples/lab/smoke_f9_phy_matrix_ota.py --cycles 1 2>&1 | tail -10
```

Expected: cycle 1 6/6 PASS (the F9 hot-switch should still work end-to-end with F22 added).

- [ ] **Step 4: Write memory note**

Create `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f22_done.md`:

```markdown
---
name: F22 Test modes CW + PRBS — closed
description: 2026-04-29 commit <HEAD_SHA>: F22 closed. Three commands (CMD_TX_CW=0x55, CMD_TX_PRBS=0x56, CMD_TX_TEST_STOP=0x57) wired through firmware (rfc_CMD_TX_TEST) and Python (radio.tx_cw/tx_prbs/tx_test_stop). 5/5 hardware smoke PASS, 9/9 unit tests PASS, no F9 regression.
type: project
---
F22 closed wire-level 2026-04-29 on branch `feature/f22-test-modes`. Tag `v2.0-f22` ready (pending Sabas approval).

## What landed

| Layer | Deliverable | File |
|---|---|---|
| Firmware | `RadioIF_runTxTest(mode)` + `RadioIF_stopTxTest()` | `firmware/cc1352/src/radio_if.c` |
| Firmware | 3 command handlers (CMD_TX_CW=0x55, CMD_TX_PRBS=0x56, CMD_TX_TEST_STOP=0x57) | `firmware/cc1352/src/command_processor.c` |
| Python | `radio.tx_cw(power_dbm)`, `radio.tx_prbs(power_dbm, pattern)`, `radio.tx_test_stop()` | `python/feralrf/radio.py` |
| Tests | 9 unit tests (command ids, payloads, ValueError, idempotent) | `python/tests/test_tx_test.py` |
| Smoke | 5 hardware tests on 2 boards (CW status, BLE band, PRBS-9, PRBS-15, idempotent) | `python/examples/lab/smoke_f22_tx_test.py` |

## Validation evidence

(fill in after smoke run with actual numbers)

## How to use

```python
from feralrf import Radio, PHY
r = Radio('/dev/ttyACM5'); r.connect(); r.init()

r.set_phy(PHY.SUB_1GHZ_868)
r.tx_cw(power_dbm=5)              # CW carrier
# ... do stuff (measure spectrum, etc.)
r.tx_test_stop()

r.tx_prbs(power_dbm=5, pattern='prbs9')  # PRBS-9 modulated
r.tx_test_stop()
```

## Out of scope (intentional)

- Spectrum analyzer validation — optional, not gating.
- TX power calibration / measurements — F23 High PA.
- Frequency override via tx_cw arg — use `set_phy(..., frequency_hz=...)` instead.
- Auto-stop timer — caller responsibility.
```

Then update `MEMORY.md` index — add line under Project section after `project_f9_done.md`:

```
- [project_f22_done.md](project_f22_done.md) — 2026-04-29: F22 Test modes CW + PRBS closed. 3 commands + 3 Python methods + 5/5 smoke PASS.
```

- [ ] **Step 5: Final status snapshot**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF
git log --oneline feature/f22-test-modes ^feature/ti-rtos-migration | wc -l
git log --oneline feature/f22-test-modes -15
git status -s
```

Capture HEAD SHA for the memory file. Commit count should be ~10 (one per task).

- [ ] **Step 6: Tag deferred**

Tag `v2.0-f22` and FF to `feature/ti-rtos-migration` are deferred to user approval — F22 closure gate met but per project pattern (F8/F8a/F9/F10/F13), tags land only on user explicit go.

---

## Self-review

**Spec coverage:**
- §1 scope (3 cmds + 3 methods + uses current PHY) → covered by Tasks 1-8
- §2 brainstorm decisions (current PHY, both PRBS, status+rx_count validation) → covered
- §3.1 Python API → Tasks 2, 3, 4
- §3.2 Firmware commands → Task 5
- §4.1 Static state → Task 7 step 1
- §4.2 Functions → Tasks 7 (run) + 8 (stop)
- §4.3 Telemetry reuse via debug_timing → smoke uses `dbg.tx_status` (Task 10)
- §5.1 Wire-level smoke 5 tests → Task 10
- §5.2 7 unit tests → split across Tasks 1-4 (4 + 1 + 3 + 1 = 9 tests; exceeds 7 because of finer granularity per test)
- §6 file layout → matches Tasks 1-10
- §7 risks → mitigated (idempotency in Task 7-8, FS pre-tune in Task 7, set_power in Tasks 2-3)
- §8 closure criteria → Task 11 step-by-step

**Placeholder scan:** none.

**Type consistency:** `Command.TX_CW`, `Command.TX_PRBS`, `Command.TX_TEST_STOP`, `RadioIF_runTxTest`, `RadioIF_stopTxTest`, `tx_cw`, `tx_prbs`, `tx_test_stop` — used identically across all tasks.

All consistent.
