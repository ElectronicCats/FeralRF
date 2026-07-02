# KillerBee Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Expose the CatSniffer (CC1352P7, via `feralrf.Radio`) as a KillerBee IEEE 802.15.4 device by adding a host-side adapter `feralrf/integrations/killerbee.py`, so KillerBee tools sniff/inject/jam through FeralRF. Zero firmware changes.

**Architecture:** KillerBee's device interface is mapped onto the existing `feralrf.Radio` API. The firmware's 802.15.4 path is already a promiscuous sniffer + raw injector (see design spec). One small host helper (`Radio.read_one_packet`) bridges KillerBee's one-packet `pnext()` to FeralRF's `read_packets()` stream.

**Tech Stack:** Python 3.9+, `feralrf` package, `pytest`. `killerbee` is an **optional** dependency (`pip install feralrf[killerbee]`), imported lazily.

## Global Constraints

- No firmware changes. No wire-protocol changes.
- Adapter lives in `python/feralrf/integrations/killerbee.py`; `killerbee` imported lazily so core FeralRF never hard-depends on it.
- Capabilities advertised: `FREQ_2400`, `SNIFF`, `SETCHAN`, `INJECT`, `PHYJAM`.
- 802.15.4 channels 11–26, page 0.
- `pnext()` returns a dict with keys `0,1,2,bytes,validcrc,rssi,dbm,location,datetime` (KillerBee tool contract) or `None`.
- Reuse `feralrf.Radio` public methods only (`set_phy, set_channel, start_rx, read_packets, stop_rx, transmit_frame, start_jam, stop_jam, list_devices, init, disconnect`). `Packet` fields: `timestamp_us, channel, rssi_dbm, lqi, crc_ok, data`.
- Follow existing test style (`FakeSerial`/dependency injection, `python/tests/test_*.py`).

---

## File Structure

- Modify `python/feralrf/radio.py` — add `Radio.read_one_packet(timeout)`.
- Create `python/feralrf/integrations/__init__.py` — empty package marker.
- Create `python/feralrf/integrations/killerbee.py` — `KillerBeeFeralRF` adapter.
- Create `python/tests/test_killerbee_integration.py` — unit tests (FakeRadio + stub caps).
- Create `python/tests/test_read_one_packet.py` — `read_one_packet` unit test (FakeSerial).
- Modify `python/pyproject.toml` — add optional `[project.optional-dependencies] killerbee`.
- Create `python/examples/killerbee_sniff.py` — direct-adapter sniff-to-pcap demo.
- Modify `docs/PYTHON_API.md` — document the integration + the KillerBee `dev_feralcat.py` shim.

---

### Task 1: `Radio.read_one_packet()`

**Files:**
- Modify: `python/feralrf/radio.py`
- Test: `python/tests/test_read_one_packet.py`

**Interfaces:**
- Consumes: existing `Radio.read_packets`, `Packet`, `RxStreamError`.
- Produces: `Radio.read_one_packet(self, timeout: float = 1.0) -> Optional[Packet]` — returns the next `Packet` within the window, skipping `RxStreamError`; `None` on timeout. Reuses `read_packets` (no parse duplication, no protocol change).

- [ ] **Step 1: Write the failing test**

```python
# python/tests/test_read_one_packet.py
from feralrf.enums import Command, Response
from feralrf.protocol import build_frame
from feralrf.radio import Radio, Packet
from tests.test_radio_strict_responses import FakeSerial  # reuse existing fake


def _rx_frame(seq, data=b"\x03\x08\xff\xff", ts=7, ch=11, rssi=200, lqi=100, crc_ok=1):
    payload = (
        ts.to_bytes(8, "little") + bytes([ch, rssi, lqi, crc_ok, len(data)]) + data
    )
    return build_frame(Response.RX_PACKET, seq, payload)


def test_read_one_packet_returns_packet():
    r = Radio(port="x")
    r._serial = FakeSerial(_rx_frame(seq=0))
    pkt = r.read_one_packet(timeout=0.5)
    assert isinstance(pkt, Packet)
    assert pkt.data == b"\x03\x08\xff\xff"
    assert pkt.channel == 11
    assert pkt.crc_ok is True
    assert pkt.rssi_dbm == 200 - 256  # signed int8


def test_read_one_packet_timeout_returns_none():
    r = Radio(port="x")
    r._serial = FakeSerial(b"")
    assert r.read_one_packet(timeout=0.05) is None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd python && pytest tests/test_read_one_packet.py -v`
Expected: FAIL — `AttributeError: 'Radio' object has no attribute 'read_one_packet'`.

- [ ] **Step 3: Write minimal implementation**

```python
# in class Radio (python/feralrf/radio.py), after read_packets():
    def read_one_packet(self, timeout: float = 1.0) -> Optional[Packet]:
        """Return the next Packet within `timeout` seconds, or None.

        Bridges KillerBee's one-packet pnext() to the read_packets() stream.
        Async RxStreamError events are skipped (they are surfaced by
        read_packets for streaming callers; a single-packet caller wants the
        next real frame). The Radio's rx buffer persists across calls, so
        abandoning the generator after one Packet loses no data.
        """
        for item in self.read_packets(timeout=timeout):
            if isinstance(item, Packet):
                return item
        return None
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd python && pytest tests/test_read_one_packet.py -v`
Expected: PASS (2 passed).

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/radio.py python/tests/test_read_one_packet.py
git commit -m "feat(radio): read_one_packet() single-frame helper for KillerBee pnext"
```

---

### Task 2: adapter package + construction + capabilities

**Files:**
- Create: `python/feralrf/integrations/__init__.py` (empty)
- Create: `python/feralrf/integrations/killerbee.py`
- Test: `python/tests/test_killerbee_integration.py`

**Interfaces:**
- Consumes: `feralrf.enums.PHY`, `feralrf.radio.Radio/Packet`, lazily `killerbee.kbutils.KBCapabilities`.
- Produces: `class KillerBeeFeralRF` with `__init__(self, dev, radio=None)` (injectable radio for tests), `get_capabilities()`, `check_capability(capab)`, `get_dev_info() -> [dev, name, fw]`, `close()`, attrs `radio`, `capabilities`, `_channel`, `_page`; module fn `_kbcaps()` returning the `KBCapabilities` class (monkeypatch point).

- [ ] **Step 1: Write the failing test**

```python
# python/tests/test_killerbee_integration.py
import feralrf.integrations.killerbee as kb
from feralrf.enums import PHY
from feralrf.radio import DeviceInfo, Packet


class FakeRadio:
    def __init__(self):
        self.phy = self.channel = None
        self.rx = False
        self.tx = []
        self.jam_ch = None
        self.jam_stopped = False
        self.queue = []
        self.disconnected = False

    def init(self):
        return DeviceInfo(firmware_version="2.0.0", capabilities=0x01, serial="00" * 8)

    def set_phy(self, phy, channel=0, frequency_hz=0):
        self.phy, self.channel = phy, channel

    def set_channel(self, ch):
        self.channel = ch

    def start_rx(self):
        self.rx = True

    def stop_rx(self):
        self.rx = False

    def read_one_packet(self, timeout=1.0):
        return self.queue.pop(0) if self.queue else None

    def transmit_frame(self, packet, timeout=5.0):
        self.tx.append(bytes(packet))

    def start_jam(self, channel, power_dbm=20, duration_ms=3000, timeout=5.0):
        self.jam_ch = channel

    def stop_jam(self, timeout=5.0):
        self.jam_stopped = True

    def disconnect(self):
        self.disconnected = True


class StubCaps:
    # class attrs mirror killerbee.kbutils.KBCapabilities constants used here
    NONE, SNIFF, SETCHAN, INJECT, PHYJAM, SELFACK, PHYJAM_REFLEX, FREQ_2400 = range(8)

    def __init__(self):
        self._d = {}

    def setcapab(self, k, v):
        self._d[k] = v

    def check(self, k):
        return self._d.get(k, False)

    def getlist(self):
        return [k for k, v in self._d.items() if v]


def _adapter(monkeypatch, radio=None):
    monkeypatch.setattr(kb, "_kbcaps", lambda: StubCaps)
    return kb.KillerBeeFeralRF(dev="/dev/fake", radio=radio or FakeRadio())


def test_construct_reports_capabilities(monkeypatch):
    a = _adapter(monkeypatch)
    assert a.check_capability(StubCaps.SNIFF)
    assert a.check_capability(StubCaps.SETCHAN)
    assert a.check_capability(StubCaps.INJECT)
    assert a.check_capability(StubCaps.PHYJAM)
    assert a.check_capability(StubCaps.FREQ_2400)


def test_dev_info_and_close(monkeypatch):
    fr = FakeRadio()
    a = _adapter(monkeypatch, fr)
    info = a.get_dev_info()
    assert isinstance(info, list) and len(info) == 3 and info[2] == "2.0.0"
    a.close()
    assert fr.disconnected
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd python && pytest tests/test_killerbee_integration.py -v`
Expected: FAIL — `ModuleNotFoundError: feralrf.integrations.killerbee`.

- [ ] **Step 3: Write minimal implementation**

```python
# python/feralrf/integrations/__init__.py
```

```python
# python/feralrf/integrations/killerbee.py
"""Expose a CatSniffer (via feralrf.Radio) as a KillerBee IEEE 802.15.4 device.

`killerbee` is an OPTIONAL dependency. It is imported lazily via _kbcaps()
so importing feralrf never requires killerbee to be installed.
"""
import time
from datetime import datetime
from typing import Optional

from feralrf.enums import PHY
from feralrf.radio import Packet, Radio


def _kbcaps():
    """Return the killerbee KBCapabilities class (lazy optional import)."""
    from killerbee.kbutils import KBCapabilities

    return KBCapabilities


class KillerBeeFeralRF:
    NAME = "FeralRF CatSniffer (CC1352)"

    def __init__(self, dev, radio: Optional[Radio] = None):
        self.dev = dev
        self.radio = radio if radio is not None else Radio(port=dev)
        self._info = self.radio.init()
        self._channel: Optional[int] = None
        self._page = 0
        KBCapabilities = _kbcaps()
        self.capabilities = KBCapabilities()
        self.__set_capabilities()

    def __set_capabilities(self):
        KBCapabilities = _kbcaps()
        c = self.capabilities
        c.setcapab(KBCapabilities.FREQ_2400, True)
        c.setcapab(KBCapabilities.SNIFF, True)
        c.setcapab(KBCapabilities.SETCHAN, True)
        c.setcapab(KBCapabilities.INJECT, True)
        c.setcapab(KBCapabilities.PHYJAM, True)

    def get_capabilities(self):
        return self.capabilities.getlist()

    def check_capability(self, capab):
        return self.capabilities.check(capab)

    def get_dev_info(self):
        return [self.dev, self.NAME, self._info.firmware_version]

    def close(self):
        try:
            self.radio.disconnect()
        except Exception:
            pass
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd python && pytest tests/test_killerbee_integration.py -v`
Expected: PASS (2 passed).

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/integrations/ python/tests/test_killerbee_integration.py
git commit -m "feat(killerbee): adapter package, construction, capabilities"
```

---

### Task 3: `set_channel`, `sniffer_on/off`, `pnext`

**Files:**
- Modify: `python/feralrf/integrations/killerbee.py`
- Test: `python/tests/test_killerbee_integration.py`

**Interfaces:**
- Produces: `set_channel(channel, page=0)`, `sniffer_on(channel=None, page=0)`, `sniffer_off()`, `pnext(timeout=100)` → KB dict or `None`.

- [ ] **Step 1: Write the failing test**

```python
# append to python/tests/test_killerbee_integration.py
import pytest


def test_set_channel_validates_and_sets_phy(monkeypatch):
    fr = FakeRadio()
    a = _adapter(monkeypatch, fr)
    a.set_channel(20)
    assert fr.phy == PHY.IEEE_802_15_4 and fr.channel == 20 and a._channel == 20
    with pytest.raises(Exception):
        a.set_channel(99)


def test_sniffer_on_starts_rx(monkeypatch):
    fr = FakeRadio()
    a = _adapter(monkeypatch, fr)
    a.sniffer_on(15)
    assert fr.phy == PHY.IEEE_802_15_4 and fr.channel == 15 and fr.rx is True


def test_pnext_maps_packet_dict(monkeypatch):
    fr = FakeRadio()
    fr.queue.append(
        Packet(timestamp_us=7, channel=15, rssi_dbm=-61, lqi=100, crc_ok=True, data=b"\x03\x08\xff\xff")
    )
    a = _adapter(monkeypatch, fr)
    a.sniffer_on(15)
    pkt = a.pnext(timeout=200)
    assert pkt["bytes"] == b"\x03\x08\xff\xff"
    assert pkt["validcrc"] is True
    assert pkt["rssi"] == -61 and pkt["dbm"] == -61
    assert pkt["location"] is None
    assert pkt[0] == b"\x03\x08\xff\xff" and pkt[1] is True and pkt[2] == -61


def test_pnext_none_on_empty(monkeypatch):
    a = _adapter(monkeypatch)
    a.sniffer_on(11)
    assert a.pnext(timeout=10) is None
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd python && pytest tests/test_killerbee_integration.py -k 'channel or sniffer or pnext' -v`
Expected: FAIL — `AttributeError: ... 'set_channel'`.

- [ ] **Step 3: Write minimal implementation**

```python
# append methods to class KillerBeeFeralRF
    def set_channel(self, channel, page=0):
        if not (11 <= channel <= 26):
            raise Exception("channel %r out of range 11-26" % channel)
        self.radio.set_phy(PHY.IEEE_802_15_4, channel)
        self.radio.set_channel(channel)
        self._channel, self._page = channel, page

    def sniffer_on(self, channel=None, page=0):
        if channel is not None:
            self.set_channel(channel, page)
        elif self._channel is None:
            self.set_channel(11)
        self.radio.start_rx()

    def sniffer_off(self):
        self.radio.stop_rx()

    def pnext(self, timeout=100):
        pkt = self.radio.read_one_packet(timeout=timeout / 1000.0)
        if pkt is None:
            return None
        return {
            0: pkt.data, 1: pkt.crc_ok, 2: pkt.rssi_dbm,
            "bytes": pkt.data, "validcrc": pkt.crc_ok,
            "rssi": pkt.rssi_dbm, "dbm": pkt.rssi_dbm,
            "location": None, "datetime": datetime.utcnow(),
        }
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd python && pytest tests/test_killerbee_integration.py -k 'channel or sniffer or pnext' -v`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/integrations/killerbee.py python/tests/test_killerbee_integration.py
git commit -m "feat(killerbee): set_channel, sniffer_on/off, pnext"
```

---

### Task 4: `inject` and `jammer_on/off`

**Files:**
- Modify: `python/feralrf/integrations/killerbee.py`
- Test: `python/tests/test_killerbee_integration.py`

**Interfaces:**
- Produces: `inject(packet, channel=None, count=1, delay=0, page=0)` (strips trailing 2-byte FCS when `len>=5`; RF core re-appends a valid one; TX `count` times, `delay` s between); `jammer_on(channel=None, page=0)` → `start_jam`; `jammer_off()` → `stop_jam`.

**Note:** FeralRF `start_jam` is duration-bounded (≤30 s). `jammer_on` starts a 30 s jam; for longer, re-arm. Documented limitation, not a bug.

- [ ] **Step 1: Write the failing test**

```python
# append to python/tests/test_killerbee_integration.py
def test_inject_strips_fcs_and_repeats(monkeypatch):
    fr = FakeRadio()
    a = _adapter(monkeypatch, fr)
    frame_with_fcs = b"\x01\x08\x00\xff\xff\xff\xff\xAA\xBB"  # last 2 = FCS
    a.inject(frame_with_fcs, channel=11, count=2)
    assert fr.channel == 11
    assert fr.tx == [b"\x01\x08\x00\xff\xff\xff\xff", b"\x01\x08\x00\xff\xff\xff\xff"]


def test_jammer_on_off_maps_to_feralrf(monkeypatch):
    fr = FakeRadio()
    a = _adapter(monkeypatch, fr)
    a.jammer_on(20)
    assert fr.jam_ch == 20
    a.jammer_off()
    assert fr.jam_stopped is True
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd python && pytest tests/test_killerbee_integration.py -k 'inject or jammer' -v`
Expected: FAIL — `AttributeError: ... 'inject'`.

- [ ] **Step 3: Write minimal implementation**

```python
# append methods to class KillerBeeFeralRF
    def inject(self, packet, channel=None, count=1, delay=0, page=0):
        if channel is not None:
            self.set_channel(channel, page)
        mpdu = packet[:-2] if len(packet) >= 5 else packet  # RF core appends FCS
        if len(mpdu) > 125:
            raise Exception("frame too long (%d > 125)" % len(mpdu))
        for i in range(count):
            self.radio.transmit_frame(mpdu)
            if delay and i < count - 1:
                time.sleep(delay)

    def jammer_on(self, channel=None, page=0):
        ch = channel if channel is not None else (self._channel or 11)
        self.set_channel(ch, page)
        self.radio.start_jam(channel=ch, duration_ms=30000)

    def jammer_off(self, channel=None, page=0):
        self.radio.stop_jam()
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cd python && pytest tests/test_killerbee_integration.py -v`
Expected: PASS (all).

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/integrations/killerbee.py python/tests/test_killerbee_integration.py
git commit -m "feat(killerbee): inject (FCS-strip) + jammer_on/off via start_jam"
```

---

### Task 5: device discovery + KillerBee registration shim + packaging

**Files:**
- Modify: `python/feralrf/integrations/killerbee.py` (add `list_devices` classmethod)
- Modify: `python/pyproject.toml` (optional `killerbee` extra)
- Test: `python/tests/test_killerbee_integration.py`

**Interfaces:**
- Produces: `KillerBeeFeralRF.list_devices() -> list[dict]` delegating to `Radio.list_devices()`.

- [ ] **Step 1: Write the failing test**

```python
# append to python/tests/test_killerbee_integration.py
def test_list_devices_delegates(monkeypatch):
    monkeypatch.setattr(
        "feralrf.radio.Radio.list_devices",
        staticmethod(lambda: [{"port": "/dev/ttyACM0", "vid": 0x1209, "pid": 0x0001}]),
    )
    devs = kb.KillerBeeFeralRF.list_devices()
    assert devs and devs[0]["port"] == "/dev/ttyACM0"
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd python && pytest tests/test_killerbee_integration.py -k list_devices -v`
Expected: FAIL — `AttributeError: ... 'list_devices'`.

- [ ] **Step 3: Write minimal implementation**

```python
# add to class KillerBeeFeralRF
    @staticmethod
    def list_devices():
        return Radio.list_devices()
```

Add to `python/pyproject.toml` (under `[project.optional-dependencies]`):

```toml
killerbee = ["killerbee>=3.0.0"]
```

Document the KillerBee-side shim (in the task's docs, not code): to make `zbid`/`zbdump -i <port>` select it, drop a `dev_feralcat.py` into the killerbee package (or ship via entry point) containing:

```python
# killerbee/dev_feralcat.py  (thin shim; core lives in feralrf)
from feralrf.integrations.killerbee import KillerBeeFeralRF as FERALCAT
```

and add a serial-probe arm in `killerbee/__init__.py` that constructs `KillerBeeFeralRF(dev)` when `KillerBeeFeralRF.list_devices()` reports that port. (Full upstream registration is out of v1 scope — see spec.)

- [ ] **Step 4: Run test to verify it passes**

Run: `cd python && pytest tests/test_killerbee_integration.py -v`
Expected: PASS (all).

- [ ] **Step 5: Commit**

```bash
git add python/feralrf/integrations/killerbee.py python/pyproject.toml python/tests/test_killerbee_integration.py
git commit -m "feat(killerbee): device discovery + optional-dep packaging"
```

---

### Task 6: example + docs

**Files:**
- Create: `python/examples/killerbee_sniff.py`
- Modify: `docs/PYTHON_API.md`

**Interfaces:** none new.

- [ ] **Step 1: Full suite green**

Run: `cd python && pytest tests/test_read_one_packet.py tests/test_killerbee_integration.py -v`
Expected: PASS (all).

- [ ] **Step 2: Write the example**

Create `python/examples/killerbee_sniff.py`: construct `KillerBeeFeralRF(dev=<port>)`, `sniffer_on(11)`, loop `pnext()` printing `bytes`/`validcrc`/`rssi`, and (optionally) write a Wireshark-loadable pcap (DLT_IEEE802_15_4). Mirror the CLI style of `smoke_phy4_ieee154.py`. State in the header that it needs `pip install feralrf[killerbee]` and real hardware.

- [ ] **Step 3: Document in PYTHON_API.md**

Add a "KillerBee integration" section: the adapter path, the capability set, the `pnext` dict contract, the `read_one_packet` bridge, the jam duration caveat, and the `dev_feralcat.py` shim. Link the design spec.

- [ ] **Step 4: Commit**

```bash
git add python/examples/killerbee_sniff.py docs/PYTHON_API.md
git commit -m "docs(killerbee): sniff example + PYTHON_API integration section"
```

---

## Self-Review

- **Spec coverage:** adapter mapping table → Tasks 2–5; `read_one_packet` bridge → Task 1; capabilities incl. PHYJAM → Task 2 + 4; detection → Task 5; testing (unit) → all; example/docs → Task 6. No firmware tasks (spec: zero firmware changes). Hardware validation is a post-merge `-m hardware` activity, noted in the spec, not a plan task.
- **Placeholder scan:** no `TBD`/"add error handling"/"write tests for the above"; every code step is complete. The KillerBee-side `dev_feralcat.py` shim is shown in full; deeper upstream registration is explicitly out of v1 scope, not a hidden gap.
- **Type consistency:** `FakeRadio` methods match the real `Radio` signatures used (`set_phy`, `set_channel`, `start_rx/stop_rx`, `read_one_packet`, `transmit_frame`, `start_jam(channel,...)`, `stop_jam`, `disconnect`, `init`); `pnext` dict keys match the spec and the `Packet` field names (`data`, `crc_ok`, `rssi_dbm`); `_kbcaps()` is the single monkeypatch seam used by all tests.
