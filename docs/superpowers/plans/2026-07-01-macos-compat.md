# macOS Compatibility Patch — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development. Steps use checkbox (`- [ ]`) syntax.

**Goal:** Make the CatSniffer-via-KillerBee stack usable on macOS. Spans two repos/branches:
- KillerBee fork `wero1414/killerbee`, branch `macos-compat` (off `catsniffer-integration`)
- FeralRF `ElectronicCats/FeralRF`, branch `feature/macos-compat` (off `feature/killerbee-integration`)

**Tech Stack:** Python 3, pyserial, pytest. Shared venv at `/Users/wero1414/zigbeepollo/FeralRF/python/.venv` (has feralrf + killerbee path-linked + pytest). Tests run with that venv's python.

## Global Constraints
- macOS CDC-ACM serial devices enumerate as `/dev/cu.usbmodem*` (open this node) and `/dev/tty.usbmodem*` — NOT `ttyACM*`/`tty.usbserial*`.
- Preserve existing Linux behavior in every change (add macOS support, don't replace).
- Every change is unit-testable without hardware EXCEPT the real on-Mac shell-port identification (Task B), which is best-effort + fallback and flagged for hardware verification.
- No firmware changes.

---

### Task A: KillerBee macOS serial detection + USB-backend guard

**Repo/branch:** `/Users/wero1414/zigbeepollo/killerbee`, branch `macos-compat`
**File:** `killerbee/kbutils.py`
**Test:** `killerbee/tests/test_macos_detection.py` (new)

**A1 — macOS serial globs.** In `get_serial_ports()` add the macOS usbmodem nodes. Current:
```python
    seriallist = glob.glob("/dev/ttyUSB*") + glob.glob("/dev/tty.usbserial*") + glob.glob("/dev/ttyACM*")
```
Change to (append the two macOS patterns):
```python
    seriallist = (glob.glob("/dev/ttyUSB*") + glob.glob("/dev/tty.usbserial*")
                  + glob.glob("/dev/ttyACM*") + glob.glob("/dev/cu.usbmodem*")
                  + glob.glob("/dev/tty.usbmodem*"))
```

**A2 — USB-backend guard.** In `devlist_usb_v1x()` the `usb.core.find(...)` call can raise `usb.core.NoBackendError` when libusb is absent (common on macOS), which crashes `zbid`/`devlist` before serial devices are ever enumerated. Guard it so it degrades to "no USB devices" and serial detection still runs:
```python
    try:
        devs: Any = usb.core.find(find_all=True, custom_match=findFromList(vendor, product))
    except usb.core.NoBackendError:
        return []   # no libusb backend (e.g. macOS without `brew install libusb`); serial devices still enumerate
```
(Keep the existing `for dev in devs:` loop and its `USBError` handling unchanged, after this.)

- [ ] **Step 1: Write failing tests**
```python
# killerbee/tests/test_macos_detection.py
import glob as _glob
from killerbee import kbutils


def test_get_serial_ports_queries_macos_usbmodem(monkeypatch):
    seen = []
    def fake_glob(pat):
        seen.append(pat)
        return []
    monkeypatch.setattr(kbutils.glob, "glob", fake_glob)
    kbutils.get_serial_ports()
    assert "/dev/cu.usbmodem*" in seen
    assert "/dev/tty.usbmodem*" in seen


def test_devlist_survives_no_usb_backend(monkeypatch):
    import usb.core
    def boom(*a, **k):
        raise usb.core.NoBackendError("no libusb")
    monkeypatch.setattr(usb.core, "find", boom)
    # only a CatSniffer on a macOS-style port, no USB dongles
    monkeypatch.setattr(kbutils, "get_serial_ports",
                        lambda include=None: ["/dev/cu.usbmodem1101"])
    monkeypatch.setattr(kbutils, "iscatsniffer",
                        lambda dev: dev == "/dev/cu.usbmodem1101")
    dl = kbutils.devlist()  # must NOT raise
    assert any(e[0] == "/dev/cu.usbmodem1101" and "CatSniffer" in e[1] for e in dl)
```

- [ ] **Step 2: Run — expect FAIL** (`/dev/cu.usbmodem*` not queried; `devlist` raises NoBackendError)
Run: `cd /Users/wero1414/zigbeepollo/killerbee && /Users/wero1414/zigbeepollo/FeralRF/python/.venv/bin/python -m pytest tests/test_macos_detection.py -v`

- [ ] **Step 3: Implement A1 + A2** (as above).

- [ ] **Step 4: Run — expect PASS.**

- [ ] **Step 5: Commit** on branch `macos-compat`:
```bash
git commit -am "feat(macos): enumerate cu/tty.usbmodem serial ports + tolerate missing USB backend"
```

---

### Task B: FeralRF macOS-aware shell-port derivation for reset_device

**Repo/branch:** `/Users/wero1414/zigbeepollo/FeralRF`, branch `feature/macos-compat`
**File:** `python/feralrf/radio.py` (`_get_shell_port`)
**Test:** `python/tests/test_shell_port.py` (new)

**Problem:** `_get_shell_port()` derives the RP2040 shell port by taking the bridge port's trailing number and adding 2 (`/dev/ttyACM0` → `/dev/ttyACM2`). That is Linux `ttyACM` sequential numbering. On macOS the port is `/dev/cu.usbmodem<serial><iface>` and the trailing digits are not a sequential index, so `+2` yields a bogus name and `reset_device()` fails (currently swallowed → reset silently skipped).

**Change:** make `_get_shell_port()` try, in order:
1. **list_ports sibling match (macOS + robust everywhere):** enumerate `serial.tools.list_ports.comports()`; find the port whose `serial_number` (or `location` USB-path prefix) matches the current bridge port's, but is a *different* device node — that sibling is the shell/console CDC. If exactly one other CDC interface of the same physical device exists, return it.
2. **Numeric offset fallback (existing Linux behavior):** if list_ports can't disambiguate (e.g. `ttyACM` where siblings share no serial_number), fall back to the current trailing-number `+2` logic so Linux is unchanged.

Keep the method's existing signature and the `ConnectionError` it raises when nothing can be derived.

> **CAVEAT (put in a code comment + the report):** the exact CatSniffer CDC-interface layout on macOS is unverified without the hardware. The sibling-match is a best guess; `reset_device()` already swallows failures, so a wrong guess degrades to "no reset" (same as today), never a crash.

- [ ] **Step 1: Write failing tests** using a fake `comports()` entry type with `.device`, `.serial_number`, `.location`:
```python
# python/tests/test_shell_port.py
import types
import pytest
from feralrf.radio import Radio


def _port(device, serial_number=None, location=None):
    return types.SimpleNamespace(device=device, serial_number=serial_number, location=location)


def test_shell_port_macos_sibling_match(monkeypatch):
    ports = [
        _port("/dev/cu.usbmodem1101", serial_number="E4B3", location="0-1.1:1.0"),  # bridge
        _port("/dev/cu.usbmodem1103", serial_number="E4B3", location="0-1.1:1.2"),  # shell (same serial)
        _port("/dev/cu.usbmodem9999", serial_number="ZZZZ", location="0-2:1.0"),     # unrelated
    ]
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: ports)
    r = Radio(port="/dev/cu.usbmodem1101")
    assert r._get_shell_port() == "/dev/cu.usbmodem1103"


def test_shell_port_linux_offset_fallback(monkeypatch):
    # ttyACM siblings share no serial_number -> fall back to +2
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: [])
    r = Radio(port="/dev/ttyACM0")
    assert r._get_shell_port() == "/dev/ttyACM2"
```

- [ ] **Step 2: Run — expect FAIL.**
Run: `cd /Users/wero1414/zigbeepollo/FeralRF/python && ./.venv/bin/python -m pytest tests/test_shell_port.py -v`

- [ ] **Step 3: Implement** the two-strategy `_get_shell_port()` (sibling-match then numeric fallback), preserving the raise-on-failure contract.

- [ ] **Step 4: Run — expect PASS**, and the full suite stays green:
`./.venv/bin/python -m pytest -q -m "not hardware and not hardware_ble"`

- [ ] **Step 5: Commit** on branch `feature/macos-compat`:
```bash
git commit -am "feat(macos): derive RP2040 shell port via list_ports sibling match, Linux +offset fallback"
```

---

## Self-Review
- Coverage: A1 macOS globs (test), A2 backend guard (test), B sibling-match + fallback (2 tests). Linux behavior preserved (A appends; B falls back to +2).
- Hardware caveat: only Task B's real-device correctness is unverifiable here; documented, degrades gracefully.
- No firmware changes.
