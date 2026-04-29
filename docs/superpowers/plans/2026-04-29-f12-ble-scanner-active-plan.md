# F12 BLE Scanner Activo — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose `radio.scan_ble_active(duration, channels, phy)` in Python that sends SCAN_REQ and merges ADV + SCAN_RSP per MAC into a typed `BleScanResult`.

**Architecture:** Python-only port. The TI-RTOS firmware already supports active scan (Scanner config + bActiveScan flag + LL classification of SCAN_RSP=PDU 0x04). This plan adds: (1) `_ble_scan.py` with `BleScanResult` dataclass + AD parser + PDU layout helpers; (2) `Radio.scan_ble_active()` method orchestrating side-effects; (3) demo + smoke + 17 unit tests.

**Tech Stack:** Python 3.10+, dataclasses, pyserial-asyncio (existing). No firmware build, no new deps.

**Spec:** `docs/superpowers/specs/2026-04-29-f12-ble-scanner-active-design.md`

**Branch:** `feature/f12-ble-scanner-active` (already created from `feature/ti-rtos-migration` HEAD `fe1937f`).

**Pre-requisites verified before starting:**
- F11a closed, capture_and_replay seq=0xFF bug fixed (commit `9e2f5f3`).
- Bloque D added to plan-v2 spec (commit `fe1937f`).
- `feature/ti-rtos-migration` is at `fe1937f` with all the above FF'd in.

---

## File Structure

Files to create:

| File | Responsibility | LOC est. |
|---|---|---|
| `python/feralrf/_ble_scan.py` | `BleScanResult` dataclass + `parse_ad_structures()` + `extract_pdu_header()` helper | ~280 |
| `python/tests/test_ble_scan.py` | 17 unit tests for parser + dataclass merge + PDU layout | ~250 |
| `python/examples/lab/demo_ble_scan_active.py` | Interactive passive-vs-active diff demo | ~150 |
| `python/examples/lab/smoke_f12_scan_active.py` | Wire-level smoke for closure gate | ~70 |

Files to modify:

| File | Change | LOC est. |
|---|---|---|
| `python/feralrf/radio.py` | Add `scan_ble_active()` method, register in class API list | +50 |

Total: ~800 LOC new, ~50 LOC modified, 0 LOC firmware.

---

## Task 1: Skeleton — empty dataclass + empty parser

**Files:**
- Create: `python/feralrf/_ble_scan.py`
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing test**

```python
# python/tests/test_ble_scan.py
"""Unit tests for BLE active scanner support (F12).

The firmware reserves PDU type 0x04 for SCAN_RSP and the LL classifier
already tags it; these tests cover the Python-side parser, dataclass,
and merge logic that consume those packets.
"""

from feralrf._ble_scan import BleScanResult, parse_ad_structures


def test_blescanresult_minimal_fields():
    r = BleScanResult(mac="DE:AD:BE:EF:CA:FE", addr_type="public")
    assert r.mac == "DE:AD:BE:EF:CA:FE"
    assert r.addr_type == "public"
    assert r.name is None
    assert r.adv_count == 0
    assert r.scan_rsp_count == 0
    assert r.uuids_16bit == []
    assert r.uuids_128bit == []
    assert r.manufacturer_data == {}
    assert r.raw_advs == []
    assert r.raw_scan_rsps == []


def test_parse_ad_empty_payload_returns_empty_dict():
    assert parse_ad_structures(b"") == {}
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: `ImportError: cannot import name 'BleScanResult' from 'feralrf._ble_scan'`

- [ ] **Step 3: Write minimal implementation**

```python
# python/feralrf/_ble_scan.py
"""BLE active scanner support — dataclass, AD parser, PDU layout helpers.

Consumed by feralrf.radio.Radio.scan_ble_active(). The TI-RTOS firmware
emits ADV_* and SCAN_RSP packets through the same data queue with
ll_pdu_type=0x04 distinguishing SCAN_RSP. This module merges them per MAC
into BleScanResult and decodes the AD structures.
"""

from dataclasses import dataclass, field
from typing import Optional


@dataclass
class BleScanResult:
    mac: str
    addr_type: str
    name: Optional[str] = None
    rssi_max: int = -128
    rssi_min: int = 0
    rssi_avg: float = 0.0
    adv_count: int = 0
    scan_rsp_count: int = 0
    flags: Optional[int] = None
    uuids_16bit: list = field(default_factory=list)
    uuids_128bit: list = field(default_factory=list)
    services_uuid16_data: dict = field(default_factory=dict)
    manufacturer_data: dict = field(default_factory=dict)
    tx_power: Optional[int] = None
    appearance: Optional[int] = None
    raw_advs: list = field(default_factory=list)
    raw_scan_rsps: list = field(default_factory=list)


def parse_ad_structures(payload: bytes) -> dict:
    """Parse BLE advertising data structures.

    Returns a dict with keys present only for AD types found in payload.
    Malformed length fields and unknown AD types are skipped silently.
    Never raises.
    """
    return {}
```

- [ ] **Step 4: Run test to verify it passes**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 2 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): skeleton BleScanResult dataclass + parse_ad_structures stub"
```

---

## Task 2: AD parser — Flags (0x01), TX Power (0x0A), Appearance (0x19)

**Files:**
- Modify: `python/feralrf/_ble_scan.py:parse_ad_structures`
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing tests**

Append to `python/tests/test_ble_scan.py`:

```python
def test_parse_ad_flags():
    # AD: len=2, type=0x01 (Flags), value=0x06
    payload = bytes([0x02, 0x01, 0x06])
    out = parse_ad_structures(payload)
    assert out == {"flags": 0x06}


def test_parse_ad_tx_power_signed():
    # AD: len=2, type=0x0A (TX Power), value=-12 (0xF4 as signed int8)
    payload = bytes([0x02, 0x0A, 0xF4])
    out = parse_ad_structures(payload)
    assert out == {"tx_power": -12}


def test_parse_ad_appearance_uint16_le():
    # AD: len=3, type=0x19 (Appearance), value=0x0040 (Generic Phone)
    payload = bytes([0x03, 0x19, 0x40, 0x00])
    out = parse_ad_structures(payload)
    assert out == {"appearance": 0x0040}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 3 NEW FAIL (existing 2 still pass). Failure: each returns `{}` instead of expected.

- [ ] **Step 3: Write minimal implementation**

Replace `parse_ad_structures` body:

```python
def parse_ad_structures(payload: bytes) -> dict:
    """Parse BLE advertising data structures.

    Returns a dict with keys present only for AD types found in payload.
    Malformed length fields and unknown AD types are skipped silently.
    Never raises.
    """
    out: dict = {}
    i = 0
    n = len(payload)
    while i < n:
        ad_len = payload[i]
        if ad_len == 0:
            i += 1
            continue
        if i + 1 + ad_len > n:
            break  # truncated
        ad_type = payload[i + 1]
        value = payload[i + 2 : i + 1 + ad_len]

        if ad_type == 0x01 and len(value) >= 1:
            out["flags"] = value[0]
        elif ad_type == 0x0A and len(value) >= 1:
            out["tx_power"] = int.from_bytes(value[:1], "little", signed=True)
        elif ad_type == 0x19 and len(value) >= 2:
            out["appearance"] = int.from_bytes(value[:2], "little")

        i += 1 + ad_len
    return out
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 5 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): AD parser handles Flags, TX Power, Appearance"
```

---

## Task 3: AD parser — Local Name (0x08 Shortened, 0x09 Complete)

**Files:**
- Modify: `python/feralrf/_ble_scan.py:parse_ad_structures`
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing tests**

Append:

```python
def test_parse_ad_complete_name_utf8():
    name = "Soundcore Boom 2"
    name_bytes = name.encode("utf-8")
    payload = bytes([len(name_bytes) + 1, 0x09]) + name_bytes
    out = parse_ad_structures(payload)
    assert out == {"name": "Soundcore Boom 2"}


def test_parse_ad_shortened_name_used_when_no_complete():
    # AD len = 1 (type) + 7 (PixelXL bytes) = 8 = 0x08
    payload = bytes([0x08, 0x08]) + b"PixelXL"
    out = parse_ad_structures(payload)
    assert out == {"name": "PixelXL"}


def test_parse_ad_complete_name_preferred_over_shortened():
    # Shortened first, then Complete — Complete should win
    # AD lens: shortened = 1+5 = 0x06, complete = 1+11 = 0x0C
    payload = (
        bytes([0x06, 0x08]) + b"Pixel"           # shortened
        + bytes([0x0C, 0x09]) + b"Pixel 7 Pro"   # complete
    )
    out = parse_ad_structures(payload)
    assert out == {"name": "Pixel 7 Pro"}


def test_parse_ad_name_invalid_utf8_replaced():
    # 0xFF is invalid UTF-8 start byte — should be replaced not raise
    payload = bytes([0x04, 0x09, 0x41, 0xFF, 0x42])
    out = parse_ad_structures(payload)
    assert "name" in out
    assert out["name"].startswith("A") and out["name"].endswith("B")
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 4 NEW FAIL.

- [ ] **Step 3: Write minimal implementation**

In `parse_ad_structures`, add after the appearance branch:

```python
        elif ad_type == 0x09:
            out["name"] = value.decode("utf-8", errors="replace")
        elif ad_type == 0x08 and "name" not in out:
            out["name"] = value.decode("utf-8", errors="replace")
```

Note: order matters. Use `elif` so name handling is in the same chain. Place 0x09 BEFORE 0x08 so when both appear in the SAME parse, the 0x09 path runs first; but also we explicitly skip 0x08 if `"name"` is already set. Order in payload still matters: if 0x08 comes before 0x09, the 0x09 branch overwrites correctly because `out["name"]` already set is overwritten unconditionally by 0x09.

Wait — to support "0x08 first then 0x09 wins" AND "0x09 first then 0x08 ignored", we need:
- 0x09 always overwrites (preferred type)
- 0x08 only sets if `"name"` not yet present

Replace the two branches above with this exact logic:

```python
        elif ad_type == 0x09:
            out["name"] = value.decode("utf-8", errors="replace")
        elif ad_type == 0x08:
            if "name" not in out:
                out["name"] = value.decode("utf-8", errors="replace")
```

This handles both orderings correctly.

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 9 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): AD parser handles Complete + Shortened Local Name with preference"
```

---

## Task 4: AD parser — 16-bit UUIDs (0x02, 0x03)

**Files:**
- Modify: `python/feralrf/_ble_scan.py:parse_ad_structures`
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing tests**

Append:

```python
def test_parse_ad_uuids_16bit_complete():
    # AD type 0x03: complete 16-bit UUID list. Two UUIDs: 0xFE2C, 0x180A
    # Little-endian on wire.
    payload = bytes([0x05, 0x03, 0x2C, 0xFE, 0x0A, 0x18])
    out = parse_ad_structures(payload)
    assert out == {"uuids_16bit": ["FE2C", "180A"]}


def test_parse_ad_uuids_16bit_incomplete_extends():
    # AD 0x02 incomplete UUID list — same handling, also added.
    payload = bytes([0x03, 0x02, 0x2C, 0xFE])
    out = parse_ad_structures(payload)
    assert out == {"uuids_16bit": ["FE2C"]}


def test_parse_ad_uuids_16bit_combined_complete_and_incomplete():
    # Both types in same payload — both extend the same list, in order.
    payload = (
        bytes([0x03, 0x02, 0x2C, 0xFE])     # incomplete: FE2C
        + bytes([0x03, 0x03, 0x0A, 0x18])   # complete:   180A
    )
    out = parse_ad_structures(payload)
    assert out == {"uuids_16bit": ["FE2C", "180A"]}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 3 NEW FAIL.

- [ ] **Step 3: Write minimal implementation**

In `parse_ad_structures`, add after the name branches:

```python
        elif ad_type in (0x02, 0x03):
            uuids = out.setdefault("uuids_16bit", [])
            for j in range(0, len(value) - 1, 2):
                uuid_int = int.from_bytes(value[j : j + 2], "little")
                uuids.append(f"{uuid_int:04X}")
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 12 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): AD parser handles Incomplete + Complete 16-bit UUID lists"
```

---

## Task 5: AD parser — 128-bit UUIDs (0x06, 0x07)

**Files:**
- Modify: `python/feralrf/_ble_scan.py:parse_ad_structures`
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing tests**

Append:

```python
def test_parse_ad_uuids_128bit_complete_canonical_format():
    # 0x07 complete 128-bit UUID list. UUID 0000180A-0000-1000-8000-00805F9B34FB
    # On wire: 16 bytes little-endian = reversed canonical bytes.
    canonical = "0000180a-0000-1000-8000-00805f9b34fb"
    canonical_hex = canonical.replace("-", "")
    wire_bytes = bytes.fromhex(canonical_hex)[::-1]  # little-endian
    payload = bytes([0x11, 0x07]) + wire_bytes
    out = parse_ad_structures(payload)
    assert out == {"uuids_128bit": [canonical]}


def test_parse_ad_uuids_128bit_incomplete_extends():
    canonical = "12345678-1234-5678-1234-567812345678"
    wire = bytes.fromhex(canonical.replace("-", ""))[::-1]
    payload = bytes([0x11, 0x06]) + wire
    out = parse_ad_structures(payload)
    assert out == {"uuids_128bit": [canonical]}
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 2 NEW FAIL.

- [ ] **Step 3: Write minimal implementation**

In `parse_ad_structures`, add after the 16-bit UUIDs branch:

```python
        elif ad_type in (0x06, 0x07):
            uuids = out.setdefault("uuids_128bit", [])
            for j in range(0, len(value) - 15, 16):
                rev = value[j : j + 16][::-1]
                hex_str = rev.hex()
                canonical = (
                    f"{hex_str[0:8]}-{hex_str[8:12]}-{hex_str[12:16]}-"
                    f"{hex_str[16:20]}-{hex_str[20:32]}"
                )
                uuids.append(canonical)
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 14 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): AD parser handles 128-bit UUIDs with canonical formatting"
```

---

## Task 6: AD parser — Service Data 16-bit UUID (0x16)

**Files:**
- Modify: `python/feralrf/_ble_scan.py:parse_ad_structures`
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing test**

Append:

```python
def test_parse_ad_service_data_uuid16():
    # 0x16: UUID (2B LE) + variable data. UUID FE2C, data 8F95F8.
    payload = bytes([0x06, 0x16, 0x2C, 0xFE, 0x8F, 0x95, 0xF8])
    out = parse_ad_structures(payload)
    assert out == {"services_uuid16_data": {"FE2C": b"\x8F\x95\xF8"}}


def test_parse_ad_service_data_multiple_uuids():
    payload = (
        bytes([0x06, 0x16, 0x2C, 0xFE, 0x01, 0x02, 0x03])
        + bytes([0x05, 0x16, 0x0A, 0x18, 0xAA, 0xBB])
    )
    out = parse_ad_structures(payload)
    assert out == {
        "services_uuid16_data": {
            "FE2C": b"\x01\x02\x03",
            "180A": b"\xAA\xBB",
        }
    }
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 2 NEW FAIL.

- [ ] **Step 3: Write minimal implementation**

In `parse_ad_structures`, add after 128-bit UUID branch:

```python
        elif ad_type == 0x16 and len(value) >= 2:
            uuid_int = int.from_bytes(value[:2], "little")
            uuid_str = f"{uuid_int:04X}"
            data = bytes(value[2:])
            sd = out.setdefault("services_uuid16_data", {})
            sd[uuid_str] = data
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 16 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): AD parser handles Service Data 16-bit UUID (0x16)"
```

---

## Task 7: AD parser — Manufacturer Data (0xFF)

**Files:**
- Modify: `python/feralrf/_ble_scan.py:parse_ad_structures`
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing test**

Append:

```python
def test_parse_ad_manufacturer_data_apple():
    # Apple company ID 0x004C (LE: 0x4C 0x00). Proximity Pairing data.
    payload = bytes([0x05, 0xFF, 0x4C, 0x00, 0x07, 0x19])
    out = parse_ad_structures(payload)
    assert out == {"manufacturer_data": {0x004C: b"\x07\x19"}}


def test_parse_ad_manufacturer_data_multiple_companies():
    payload = (
        bytes([0x05, 0xFF, 0x4C, 0x00, 0x07, 0x19])     # Apple
        + bytes([0x05, 0xFF, 0xF4, 0x2B, 0xAA, 0xBB])   # Anker
    )
    out = parse_ad_structures(payload)
    assert out == {
        "manufacturer_data": {
            0x004C: b"\x07\x19",
            0x2BF4: b"\xAA\xBB",
        }
    }


def test_parse_ad_manufacturer_data_too_short_skipped():
    # AD 0xFF with only 1 byte of value (no full company_id) — skipped.
    payload = bytes([0x02, 0xFF, 0x4C])
    out = parse_ad_structures(payload)
    assert "manufacturer_data" not in out
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 3 NEW FAIL.

- [ ] **Step 3: Write minimal implementation**

In `parse_ad_structures`, add after Service Data branch:

```python
        elif ad_type == 0xFF and len(value) >= 2:
            company_id = int.from_bytes(value[:2], "little")
            data = bytes(value[2:])
            md = out.setdefault("manufacturer_data", {})
            md[company_id] = data
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 19 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): AD parser handles Manufacturer Data (0xFF)"
```

---

## Task 8: AD parser — malformed handling

**Files:**
- Test: `python/tests/test_ble_scan.py` (impl already correct — verify edge cases)

- [ ] **Step 1: Write the failing tests**

Append:

```python
def test_parse_ad_zero_length_skipped_no_infinite_loop():
    # AD len=0 followed by valid AD — must not infinite-loop.
    payload = bytes([0x00, 0x02, 0x01, 0x06])
    out = parse_ad_structures(payload)
    assert out == {"flags": 0x06}


def test_parse_ad_truncated_length_breaks_cleanly():
    # AD claims len=10 but only 3 bytes follow — break, don't raise.
    payload = bytes([0x0A, 0x09, 0x41, 0x42, 0x43])
    out = parse_ad_structures(payload)
    assert out == {}


def test_parse_ad_unknown_type_skipped_correctly():
    # Unknown AD type 0xAB followed by valid Flags.
    payload = bytes([0x03, 0xAB, 0xFF, 0xFF]) + bytes([0x02, 0x01, 0x06])
    out = parse_ad_structures(payload)
    assert out == {"flags": 0x06}
```

- [ ] **Step 2: Run tests to verify they pass (current impl handles these)**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 22 PASSED. (No impl change — this verifies the existing parser already handles malformed cases. If any FAILS, fix the parser to satisfy them, then proceed.)

- [ ] **Step 3: Commit**

```bash
git add python/tests/test_ble_scan.py
git commit -m "test(f12): regression coverage for malformed AD structures"
```

---

## Task 9: PDU header — extract MAC + addr_type

**Files:**
- Modify: `python/feralrf/_ble_scan.py` (add `extract_pdu_header()`)
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing tests**

Append:

```python
from feralrf._ble_scan import extract_pdu_header


def test_extract_pdu_header_public_address():
    # PDU header [type|RFU][len|RxAdd|TxAdd] + AdvA (6B LE)
    # TxAdd=0 → public. AdvA = DE AD BE EF CA FE display, wire LE = FE CA EF BE AD DE.
    pkt_data = bytes([0x00, 0x06, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE])
    mac, addr_type = extract_pdu_header(pkt_data)
    assert mac == "DE:AD:BE:EF:CA:FE"
    assert addr_type == "public"


def test_extract_pdu_header_random_static():
    # TxAdd=1 (bit 6 of byte 1 = 0x40) + AdvA byte 5 high bits = 0xC0 → static
    pkt_data = bytes([0x00, 0x46, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE | 0xC0])
    mac, addr_type = extract_pdu_header(pkt_data)
    expected_msb = 0xDE | 0xC0
    assert mac == f"{expected_msb:02X}:AD:BE:EF:CA:FE"
    assert addr_type == "random_static"


def test_extract_pdu_header_random_resolvable():
    # TxAdd=1 + high bits 0b01 (0x40)
    pkt_data = bytes([0x00, 0x46, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0x7E])  # 0x7E high bits = 01
    mac, addr_type = extract_pdu_header(pkt_data)
    assert addr_type == "random_resolvable"


def test_extract_pdu_header_random_non_resolvable():
    # TxAdd=1 + high bits 0b00
    pkt_data = bytes([0x00, 0x46, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0x3E])  # 0x3E high bits = 00
    mac, addr_type = extract_pdu_header(pkt_data)
    assert addr_type == "random_non_resolvable"


def test_extract_pdu_header_too_short_returns_none():
    pkt_data = bytes([0x00, 0x06, 0xFE])  # truncated
    mac, addr_type = extract_pdu_header(pkt_data)
    assert mac is None and addr_type is None
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 5 NEW FAIL with `ImportError: cannot import name 'extract_pdu_header'`.

- [ ] **Step 3: Write minimal implementation**

Append to `python/feralrf/_ble_scan.py`:

```python
def extract_pdu_header(pkt_data: bytes) -> tuple:
    """Extract (mac_display_str, addr_type_str) from a BLE adv-channel PDU.

    Layout: [PDU header (2B)] [AdvA (6B little-endian)] [AdvData ...]

    PDU header byte 1 bit 6 = TxAdd:
      0 → public address
      1 → random; sub-classify by AdvA[5] high 2 bits:
            0b00 → random_non_resolvable
            0b01 → random_resolvable
            0b11 → random_static
            0b10 → reserved (treat as random_non_resolvable)

    Returns (None, None) if pkt_data is shorter than 8 bytes.
    """
    if len(pkt_data) < 8:
        return (None, None)
    tx_add = (pkt_data[1] >> 6) & 0x01
    adva_le = pkt_data[2:8]
    mac = ":".join(f"{b:02X}" for b in reversed(adva_le))
    if tx_add == 0:
        addr_type = "public"
    else:
        high2 = (adva_le[5] >> 6) & 0x03
        if high2 == 0b11:
            addr_type = "random_static"
        elif high2 == 0b01:
            addr_type = "random_resolvable"
        else:  # 0b00 or 0b10
            addr_type = "random_non_resolvable"
    return (mac, addr_type)
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 27 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): extract_pdu_header decodes MAC + addr_type from BLE adv PDUs"
```

---

## Task 10: BleScanResult merge — adv + scan_rsp + RSSI rolling

**Files:**
- Modify: `python/feralrf/_ble_scan.py` (add `BleScanResult.update_from_packet()`)
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing tests**

Append:

```python
class FakePkt:
    """Stand-in for feralrf.radio.Packet — only the fields update_from_packet uses."""

    def __init__(self, data: bytes, rssi: int, ll_pdu_type: int):
        self.data = data
        self.rssi_dbm = rssi
        self.ll_pdu_type = ll_pdu_type
        self.crc_ok = True


def test_blescanresult_update_adv_then_scan_rsp_merges():
    # ADV_IND: PDU type 0x00. Name from ADV.
    adv_data = (
        bytes([0x00, 0x09, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE])  # header + AdvA
        + bytes([0x05, 0x09]) + b"Demo"                         # AD: complete name
    )
    # SCAN_RSP: PDU type 0x04. UUIDs only here.
    rsp_data = (
        bytes([0x04, 0x09, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE])  # header + AdvA
        + bytes([0x03, 0x03, 0x2C, 0xFE])                       # AD: complete 16-bit UUID FE2C
    )
    r = BleScanResult(mac="DE:AD:BE:EF:CA:FE", addr_type="public")
    r.update_from_packet(FakePkt(adv_data, rssi=-50, ll_pdu_type=0x00))
    r.update_from_packet(FakePkt(rsp_data, rssi=-52, ll_pdu_type=0x04))

    assert r.adv_count == 1
    assert r.scan_rsp_count == 1
    assert r.name == "Demo"
    assert r.uuids_16bit == ["FE2C"]
    assert r.rssi_max == -50
    assert r.rssi_min == -52
    assert r.rssi_avg == -51.0
    assert r.raw_advs == [adv_data[8:]]
    assert r.raw_scan_rsps == [rsp_data[8:]]


def test_blescanresult_rssi_rolling_avg_three_packets():
    r = BleScanResult(mac="DE:AD:BE:EF:CA:FE", addr_type="public")
    pkt_data = bytes([0x00, 0x06, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE])
    for rssi in (-40, -50, -60):
        r.update_from_packet(FakePkt(pkt_data, rssi=rssi, ll_pdu_type=0x00))
    assert r.rssi_max == -40
    assert r.rssi_min == -60
    assert r.rssi_avg == -50.0
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 2 NEW FAIL with `AttributeError: 'BleScanResult' object has no attribute 'update_from_packet'`.

- [ ] **Step 3: Write minimal implementation**

Add to `BleScanResult` class (inside the dataclass, after fields):

```python
    def update_from_packet(self, pkt) -> None:
        """Merge a single Packet into this result.

        pkt must have: data (bytes), rssi_dbm (int), ll_pdu_type (int).
        Caller is responsible for routing the packet to the right MAC's result.
        """
        ad_payload = pkt.data[8:]
        ad = parse_ad_structures(ad_payload)

        # 0x09 (Complete) overwrites; 0x08 only if no name yet.
        if "name" in ad:
            self.name = ad["name"]
        if "flags" in ad:
            self.flags = ad["flags"]
        if "tx_power" in ad:
            self.tx_power = ad["tx_power"]
        if "appearance" in ad:
            self.appearance = ad["appearance"]
        for u in ad.get("uuids_16bit", []):
            if u not in self.uuids_16bit:
                self.uuids_16bit.append(u)
        for u in ad.get("uuids_128bit", []):
            if u not in self.uuids_128bit:
                self.uuids_128bit.append(u)
        for k, v in ad.get("services_uuid16_data", {}).items():
            self.services_uuid16_data[k] = v
        for k, v in ad.get("manufacturer_data", {}).items():
            self.manufacturer_data[k] = v

        # RSSI rolling stats.
        n_total = self.adv_count + self.scan_rsp_count
        self.rssi_max = max(self.rssi_max, pkt.rssi_dbm)
        self.rssi_min = min(self.rssi_min, pkt.rssi_dbm) if n_total > 0 else pkt.rssi_dbm
        # Recompute avg incrementally
        new_n = n_total + 1
        self.rssi_avg = (self.rssi_avg * n_total + pkt.rssi_dbm) / new_n

        # Classify by ll_pdu_type
        if pkt.ll_pdu_type == 0x04:
            self.scan_rsp_count += 1
            self.raw_scan_rsps.append(ad_payload)
        else:
            self.adv_count += 1
            self.raw_advs.append(ad_payload)
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 29 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): BleScanResult.update_from_packet merges adv+scan_rsp with RSSI rolling stats"
```

---

## Task 11: BleScanResult.to_dict() JSON-safe

**Files:**
- Modify: `python/feralrf/_ble_scan.py` (add `to_dict()` method)
- Test: `python/tests/test_ble_scan.py`

- [ ] **Step 1: Write the failing test**

Append:

```python
import json


def test_blescanresult_to_dict_json_serializable():
    r = BleScanResult(mac="DE:AD:BE:EF:CA:FE", addr_type="public")
    pkt_data = (
        bytes([0x00, 0x09, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE])
        + bytes([0x05, 0x09]) + b"Demo"
        + bytes([0x05, 0xFF, 0x4C, 0x00, 0x07, 0x19])
    )
    r.update_from_packet(FakePkt(pkt_data, rssi=-50, ll_pdu_type=0x00))

    d = r.to_dict()
    s = json.dumps(d)  # must not raise
    parsed = json.loads(s)

    assert parsed["mac"] == "DE:AD:BE:EF:CA:FE"
    assert parsed["name"] == "Demo"
    # bytes fields became hex strings:
    assert parsed["manufacturer_data"] == {"76": "0719"}  # 0x004C as decimal str key in JSON
    assert all(isinstance(p, str) for p in parsed["raw_advs"])
```

- [ ] **Step 2: Run tests to verify they fail**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 1 NEW FAIL — `AttributeError` or `TypeError: bytes is not JSON serializable`.

- [ ] **Step 3: Write minimal implementation**

Add to `BleScanResult` class:

```python
    def to_dict(self) -> dict:
        """Return JSON-serializable dict.

        Bytes fields become hex strings. Integer dict keys become strings (JSON requirement).
        """
        from dataclasses import asdict

        d = asdict(self)
        d["raw_advs"] = [b.hex() for b in self.raw_advs]
        d["raw_scan_rsps"] = [b.hex() for b in self.raw_scan_rsps]
        d["services_uuid16_data"] = {k: v.hex() for k, v in self.services_uuid16_data.items()}
        d["manufacturer_data"] = {str(k): v.hex() for k, v in self.manufacturer_data.items()}
        return d
```

- [ ] **Step 4: Run tests to verify they pass**

```bash
.venv/bin/python -m pytest tests/test_ble_scan.py -v
```

Expected: 30 PASSED.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/_ble_scan.py python/tests/test_ble_scan.py
git commit -m "feat(f12): BleScanResult.to_dict() JSON-safe with hex-encoded bytes"
```

---

## Task 12: `Radio.scan_ble_active()` orchestration

**Files:**
- Modify: `python/feralrf/radio.py` (add method, register in class API list)

This is hardware-touching; we will not write a unit test that mocks the serial layer (existing `test_radio_strict_responses.py` style is brittle and there's already one pre-existing fail). Instead, the smoke test in Task 13 is the integration test for this method.

- [ ] **Step 1: Add the import**

In `python/feralrf/radio.py`, near the existing imports at top:

```python
from feralrf._ble_scan import BleScanResult, extract_pdu_header
```

- [ ] **Step 2: Register the method in the public API list**

Find the class-level `_PUBLIC_METHODS` (or equivalent symbol — look for the existing list near `set_ble_scan_mode` registration). The existing list around line 165 of `python/feralrf/radio.py` looks like:

```python
        "stop_rx",
        ...
        "set_ble_scan_mode",
        ...
```

Add `"scan_ble_active",` to that list, alphabetically near `"set_ble_scan_mode"`.

- [ ] **Step 3: Add the method**

Add after `set_ble_scan_mode` (around line 793):

```python
    def scan_ble_active(
        self,
        duration: float,
        channels=(37, 38, 39),
        phy: PHY = PHY.BLE_1M,
    ) -> dict:
        """Active BLE scan — send SCAN_REQ, capture SCAN_RSP, merge per MAC.

        Saves and restores set_ble_scan_mode, set_adv_hop, and PHY/channel
        on exit (try/finally), even on exception.

        Args:
            duration: seconds to listen.
            channels: int or sequence of advertising channels (37/38/39).
                      Single channel → adv_hop disabled.
                      Multiple channels → adv_hop enabled, scan starts at channels[0].
            phy: BLE PHY (default BLE_1M).

        Returns:
            dict[str, BleScanResult] keyed by MAC display string.
        """
        if isinstance(channels, int):
            channels = (channels,)
        else:
            channels = tuple(channels)
        if not channels:
            raise ValueError("channels must contain at least one channel")

        prior_phy = self._phy
        prior_channel = self._channel
        hop_needed = len(channels) > 1
        results: dict = {}

        try:
            self.set_ble_scan_mode(active=True)
            self.set_adv_hop(hop_needed)
            self.set_phy(phy, channel=channels[0])
            self.start_rx()

            for pkt in self.read_packets(timeout=duration):
                if not pkt.crc_ok:
                    continue
                if len(pkt.data) < 8:
                    continue
                # Only BLE adv-channel PDUs have ll_pdu_type set; non-BLE
                # data has ll_pdu_type None. Filter to adv/scan kinds.
                if pkt.ll_pdu_type is None:
                    continue
                # PDU types: 0x00 ADV_IND, 0x01 ADV_DIRECT, 0x02 ADV_NONCONN_IND,
                # 0x04 SCAN_RSP, 0x06 ADV_SCAN_IND, 0x07 ADV_EXT_IND.
                # 0x03 SCAN_REQ and 0x05 CONNECT_IND are not what we expect from a peripheral.
                if pkt.ll_pdu_type not in (0x00, 0x01, 0x02, 0x04, 0x06, 0x07):
                    continue

                mac, addr_type = extract_pdu_header(pkt.data)
                if mac is None:
                    continue
                result = results.get(mac)
                if result is None:
                    result = BleScanResult(mac=mac, addr_type=addr_type)
                    results[mac] = result
                result.update_from_packet(pkt)
        finally:
            try:
                self.stop_rx()
            except Exception:
                pass
            try:
                self.set_ble_scan_mode(active=False)
            except Exception:
                pass
            try:
                self.set_adv_hop(False)
            except Exception:
                pass
            try:
                if prior_phy is not None:
                    self.set_phy(prior_phy, channel=prior_channel)
            except Exception:
                pass

        return results
```

- [ ] **Step 4: Verify the existing Python test suite still passes (no new tests, but no regression)**

```bash
.venv/bin/python -m pytest 2>&1 | tail -5
```

Expected: existing pass count remains; the only failure is the pre-existing `test_read_response_ignores_echoed_command_frames` from before this branch.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/radio.py
git commit -m "feat(f12): Radio.scan_ble_active() merges ADV + SCAN_RSP per MAC with state restoration"
```

---

## Task 13: Hardware smoke — `smoke_f12_scan_active.py`

**Files:**
- Create: `python/examples/lab/smoke_f12_scan_active.py`

- [ ] **Step 1: Write the smoke script**

```python
#!/usr/bin/env python3
"""F12 wire-level smoke — active BLE scan against ambient lab traffic.

Closure criterion: ≥1 device with name + UUIDs/mfg + scan_rsp_count > 0.
If lab is RF-quiet or has no scannable peripheral in range, smoke fails
and reports — bring an ESP32/phone/smart-bulb closer and retry.

Usage:
    python smoke_f12_scan_active.py [--port /dev/ttyACM8] [--duration 10]
"""

import argparse
import sys
import time
import warnings

warnings.simplefilter("ignore")

from feralrf import Radio


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--port", default="/dev/ttyACM8")
    p.add_argument("--duration", type=float, default=10.0)
    args = p.parse_args()

    r = Radio(args.port)
    r.connect()
    time.sleep(0.3)
    r.init()

    print(f"[STEP] active scan {args.duration}s on ch 37/38/39 from {args.port}")
    t0 = time.time()
    result = r.scan_ble_active(duration=args.duration)
    dt = time.time() - t0

    n_devices = len(result)
    n_with_rsp = sum(1 for x in result.values() if x.scan_rsp_count > 0)
    closure_eligible = [
        x for x in result.values()
        if x.scan_rsp_count > 0
        and x.name
        and (x.uuids_16bit or x.uuids_128bit or x.manufacturer_data)
    ]

    print(f"[INFO] devices={n_devices}, scan_rsps={n_with_rsp}, "
          f"closure-eligible={len(closure_eligible)}, dt={dt:.1f}s")

    r.disconnect()

    if n_devices < 3:
        print(f"[FAIL] expected ≥3 BLE devices in lab ambient; got {n_devices}")
        return 1
    if n_with_rsp < 1:
        print("[FAIL] expected ≥1 device responding to SCAN_REQ; bring a scannable peripheral closer")
        return 1
    if not closure_eligible:
        print("[FAIL] F12 closure criterion not met: no device with name + UUIDs/mfg + scan_rsp")
        return 1

    print(f"[ OK ] F12 wire smoke PASS")
    for x in closure_eligible[:3]:
        uuids_total = len(x.uuids_16bit) + len(x.uuids_128bit)
        mfg_total = len(x.manufacturer_data)
        print(f"        {x.mac} '{x.name}' adv={x.adv_count} rsp={x.scan_rsp_count} "
              f"uuids={uuids_total} mfg_companies={mfg_total}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Run smoke on hardware**

```bash
cd /home/sabas/Documents/electroniccats/FeralRF/python
.venv/bin/python examples/lab/smoke_f12_scan_active.py --port /dev/ttyACM8 --duration 10
```

Expected: `[ OK ] F12 wire smoke PASS` plus a few device lines. If FAIL, read the message and resolve (bring a scannable peripheral closer or extend duration).

- [ ] **Step 3: Commit**

```bash
git add python/examples/lab/smoke_f12_scan_active.py
git commit -m "test(f12): hardware smoke validates active scan against ambient traffic"
```

---

## Task 14: Demo — `demo_ble_scan_active.py`

**Files:**
- Create: `python/examples/lab/demo_ble_scan_active.py`

- [ ] **Step 1: Write the demo**

```python
#!/usr/bin/env python3
"""F12 demo — passive vs active BLE scan, prints the delta active adds.

Auto-validates the F12 closure criterion: ≥1 device contributes scan_rsp
content (name completion or UUIDs/mfg) not present in passive ADV alone.

Usage:
    python demo_ble_scan_active.py [port] [--json out.json] [--duration 5]
"""

import argparse
import json
import sys
import time
import warnings
from collections import defaultdict
from typing import Dict

warnings.simplefilter("ignore")

from feralrf import PHY, Radio
from feralrf._ble_scan import BleScanResult, extract_pdu_header


def passive_scan(radio: Radio, duration: float) -> Dict[str, BleScanResult]:
    """Passive scan — no SCAN_REQ. Same per-MAC merge as active, just no scan_rsps."""
    radio.set_ble_scan_mode(active=False)
    radio.set_adv_hop(True)
    radio.set_phy(PHY.BLE_1M, channel=37)
    radio.start_rx()
    results: Dict[str, BleScanResult] = {}
    try:
        for pkt in radio.read_packets(timeout=duration):
            if not pkt.crc_ok or len(pkt.data) < 8 or pkt.ll_pdu_type is None:
                continue
            if pkt.ll_pdu_type not in (0x00, 0x01, 0x02, 0x06, 0x07):
                continue
            mac, addr_type = extract_pdu_header(pkt.data)
            if mac is None:
                continue
            r = results.setdefault(mac, BleScanResult(mac=mac, addr_type=addr_type))
            r.update_from_packet(pkt)
    finally:
        radio.stop_rx()
        radio.set_adv_hop(False)
    return results


def print_table(title: str, results: Dict[str, BleScanResult], show_rsp: bool):
    print(f"\n=== {title} ===")
    if not results:
        print("  (no devices)")
        return
    header = f"{'MAC':<18} {'name':<22} {'rssi':>5} {'adv':>4}"
    if show_rsp:
        header += f" {'rsp':>4}"
    header += f" {'uuids':>5} {'mfg':>4}"
    print(header)
    print("-" * len(header))
    rows = sorted(results.values(), key=lambda r: -r.rssi_max)
    for r in rows:
        name = (r.name or "(no name)")[:22]
        n_uuids = len(r.uuids_16bit) + len(r.uuids_128bit)
        n_mfg = len(r.manufacturer_data)
        line = f"{r.mac:<18} {name:<22} {r.rssi_max:>5d} {r.adv_count:>4d}"
        if show_rsp:
            line += f" {r.scan_rsp_count:>4d}"
        line += f" {n_uuids:>5d} {n_mfg:>4d}"
        print(line)


def diff_passive_vs_active(passive: Dict[str, BleScanResult], active: Dict[str, BleScanResult]):
    print("\n=== diff: what active adds (per device seen in both) ===")
    closure_pass = False
    common = set(passive.keys()) & set(active.keys())
    if not common:
        print("  (no devices in both — cannot diff)")
        return False

    for mac in sorted(common):
        p = passive[mac]
        a = active[mac]
        notes = []
        if a.name and p.name != a.name:
            notes.append(f"name '{p.name or '∅'}' → '{a.name}' ★")
            closure_pass = True
        new_uuids16 = [u for u in a.uuids_16bit if u not in p.uuids_16bit]
        new_uuids128 = [u for u in a.uuids_128bit if u not in p.uuids_128bit]
        new_mfg = [hex(c) for c in a.manufacturer_data if c not in p.manufacturer_data]
        new_svc = [u for u in a.services_uuid16_data if u not in p.services_uuid16_data]
        if new_uuids16:
            notes.append(f"UUIDs16 +{len(new_uuids16)} {new_uuids16} ★")
            closure_pass = True
        if new_uuids128:
            notes.append(f"UUIDs128 +{len(new_uuids128)} ★")
            closure_pass = True
        if new_mfg:
            notes.append(f"mfg companies +{len(new_mfg)} {new_mfg} ★")
            closure_pass = True
        if new_svc:
            notes.append(f"service-data +{len(new_svc)} ★")
            closure_pass = True
        if notes:
            print(f"  {mac} '{a.name or '?'}'")
            for n in notes:
                print(f"      {n}")
    return closure_pass


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("port", nargs="?", default="/dev/ttyACM0")
    p.add_argument("--json", dest="json_path")
    p.add_argument("--duration", type=float, default=5.0)
    args = p.parse_args()

    r = Radio(args.port)
    r.connect()
    time.sleep(0.3)
    r.init()

    try:
        print(f"[1/3] passive scan {args.duration}s on {args.port}")
        passive = passive_scan(r, args.duration)
        print_table("passive", passive, show_rsp=False)

        print(f"\n[2/3] active scan {args.duration}s on {args.port}")
        active = r.scan_ble_active(duration=args.duration)
        print_table("active", active, show_rsp=True)

        print("\n[3/3] computing diff")
        closure = diff_passive_vs_active(passive, active)

        n_p = len(passive)
        n_a = len(active)
        delta_uuids = sum(
            len(a.uuids_16bit) + len(a.uuids_128bit) for a in active.values()
        ) - sum(
            len(p.uuids_16bit) + len(p.uuids_128bit) for p in passive.values()
        )
        print(f"\nSUMMARY  passive: {n_p} devices  active: {n_a} devices  Δuuids={delta_uuids}")

        if closure:
            print("F12 closure: PASS — at least 1 device contributed scan_rsp content not in adv")
        else:
            print("F12 closure: SKIP — no scannable peripheral in range. "
                  "Bring an ESP32/phone/smart-bulb closer and re-run.")

        if args.json_path:
            out = {
                "passive": {mac: r.to_dict() for mac, r in passive.items()},
                "active": {mac: r.to_dict() for mac, r in active.items()},
                "closure_pass": closure,
            }
            with open(args.json_path, "w") as f:
                json.dump(out, f, indent=2)
            print(f"\nFull results dumped to {args.json_path}")
    finally:
        r.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

- [ ] **Step 2: Run demo on hardware**

```bash
.venv/bin/python examples/lab/demo_ble_scan_active.py /dev/ttyACM8 --duration 5
```

Expected: prints both tables and a non-empty diff section. `F12 closure: PASS` if lab has a scannable peripheral. With `--json out.json`, file written.

- [ ] **Step 3: Commit**

```bash
git add python/examples/lab/demo_ble_scan_active.py
git commit -m "feat(f12): demo compares passive vs active scan and validates closure criterion"
```

---

## Task 15: Closure gate — pre-commit + full suite + final smoke

**Files:** none (validation only)

- [ ] **Step 1: Run pre-commit on all changed files**

```bash
git diff --name-only origin/feature/ti-rtos-migration..HEAD | xargs pre-commit run --files 2>&1 | tail -15
```

Expected: all hooks PASS.

- [ ] **Step 2: Run full Python test suite**

```bash
.venv/bin/python -m pytest 2>&1 | tail -5
```

Expected: 30 new + previous total. Only fail allowed: pre-existing `test_read_response_ignores_echoed_command_frames` from before this branch.

- [ ] **Step 3: Re-run hardware smoke**

```bash
.venv/bin/python examples/lab/smoke_f12_scan_active.py --port /dev/ttyACM8 --duration 10
```

Expected: `[ OK ] F12 wire smoke PASS`.

- [ ] **Step 4: Update memory**

Write `~/.claude/projects/-home-sabas-Documents-electroniccats-FeralRF/memory/project_f12_done.md` with closure record (HEAD commit hash, smoke results, what's deferred to manual checkpoint), and add entry to `MEMORY.md` index.

- [ ] **Step 5: FF merge to consolidation branch (optional, awaiting tag)**

Per the workflow, FF `feature/f12-ble-scanner-active` → `feature/ti-rtos-migration` AFTER manual checkpoint with 3 peripherals. Until then, leave the branch local.

For now, just record the HEAD:

```bash
git log --oneline feature/f12-ble-scanner-active ^feature/ti-rtos-migration | wc -l
git log --oneline -1
```

Tag `v2.0-f12` deferred until manual checkpoint with 3 peripherals (móvil/ESP32/comercial), same pattern as F11.

---

## Self-review

**Spec coverage:**
- §1 scope (Python-only, audit confirms) → covered by Task 0 (no firmware tasks)
- §2.1 dataclass → Tasks 1, 10, 11
- §2.2 AD parser table → Tasks 2-7 (Flags/TXP/Appearance, Name, UUIDs16, UUIDs128, ServiceData, Manufacturer)
- §2.3 PDU layout → Task 9
- §2.4 scan_ble_active method → Task 12
- §2.5 cleanup semantics → Task 12 try/finally + restoration documented
- §3 demo → Task 14
- §4.1 17 unit tests → Tasks 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 — count: 2+3+4+3+2+2+3+3+5+2+1 = 30 tests (exceeds 17, finer granularity)
- §4.2 hardware smoke → Task 13
- §4.3 manual checkpoint deferred → noted in Task 15
- §5 file layout → matches Tasks 1-14
- §6 risks → mitigation: f12-r1 explicit in smoke + demo; f12-r2 documented; f12-r3 not regressed (smoke); f12-r4 base branch has fix
- §7 closure criteria → Task 15 step-by-step

**Placeholder scan:** none.

**Type consistency:**
- `BleScanResult` field names (`mac`, `addr_type`, `name`, `rssi_max/min/avg`, `adv_count`, `scan_rsp_count`, `flags`, `uuids_16bit`, `uuids_128bit`, `services_uuid16_data`, `manufacturer_data`, `tx_power`, `appearance`, `raw_advs`, `raw_scan_rsps`) — used consistently across Tasks 1, 10, 11, 12, 14.
- `parse_ad_structures(payload: bytes) -> dict` — same signature in Tasks 1-8.
- `extract_pdu_header(pkt_data: bytes) -> tuple` — same signature in Tasks 9, 12, 14.
- `update_from_packet(self, pkt) -> None` — same signature in Tasks 10, 11, 12, 14.
- `to_dict(self) -> dict` — same in Tasks 11, 14.
- `scan_ble_active(self, duration, channels=(37,38,39), phy=PHY.BLE_1M)` — same in Tasks 12, 13, 14.

All consistent.
