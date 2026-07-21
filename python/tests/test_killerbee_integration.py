import pytest

import feralrf.integrations.killerbee as kb
from feralrf.enums import PHY
from feralrf.radio import DeviceInfo, Packet


class FakeRadio:
    def __init__(self):
        self.port = "/dev/fake"
        self.phy = self.channel = None
        self.rx = False
        self.tx = []
        self.jam_ch = None
        self.jam_stopped = False
        self.queue = []
        self.disconnected = False
        self.connected = False
        self.reset_count = 0
        self.reset_before_connect = False
        self.reset_raises = False

    def connect(self):
        self.connected = True

    def reset_device(self, wait=1.5):
        if not self.connected:
            self.reset_before_connect = True
        if self.reset_raises:
            raise RuntimeError("no shell port")
        self.reset_count += 1

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


def test_reset_default_off(monkeypatch):
    # Default is reset_on_init=False: on real hardware the reset disrupts the
    # CC1352 (2026-07-02 bench), so it must not run unless opted in.
    monkeypatch.setattr(kb, "_kbcaps", lambda: StubCaps)
    fr = FakeRadio()
    kb.KillerBeeFeralRF(dev="/dev/fake", radio=fr)  # reset_on_init defaults False
    assert fr.reset_count == 0
    assert fr.connected is False


def test_reset_on_init_opt_in(monkeypatch):
    monkeypatch.setattr(kb, "_kbcaps", lambda: StubCaps)
    fr = FakeRadio()
    kb.KillerBeeFeralRF(dev="/dev/fake", radio=fr, reset_on_init=True)
    assert fr.connected is True
    assert fr.reset_count == 1
    assert fr.reset_before_connect is False  # connect() runs before reset_device()


def test_reset_failure_is_swallowed(monkeypatch):
    monkeypatch.setattr(kb, "_kbcaps", lambda: StubCaps)
    fr = FakeRadio()
    fr.reset_raises = True  # e.g. no RP2040 shell port
    # With reset opted in, a reset failure must not break construction.
    a = kb.KillerBeeFeralRF(dev="/dev/fake", radio=fr, reset_on_init=True)
    assert a.check_capability(StubCaps.INJECT)
    assert a._info.firmware_version == "2.0.0"


def test_dev_info_and_close(monkeypatch):
    fr = FakeRadio()
    a = _adapter(monkeypatch, fr)
    info = a.get_dev_info()
    assert isinstance(info, list) and len(info) == 3 and info[2] == "2.0.0"
    a.close()
    assert fr.disconnected


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
        Packet(
            timestamp_us=7, channel=15, rssi_dbm=-61, lqi=100, crc_ok=True, data=b"\x03\x08\xff\xff"
        )
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


def test_inject_strips_fcs_and_repeats(monkeypatch):
    fr = FakeRadio()
    a = _adapter(monkeypatch, fr)
    frame_with_fcs = b"\x01\x08\x00\xff\xff\xff\xff\xaa\xbb"  # last 2 = FCS
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


def test_list_devices_delegates(monkeypatch):
    monkeypatch.setattr(
        "feralrf.radio.Radio.list_devices",
        staticmethod(lambda: [{"port": "/dev/ttyACM0", "vid": 0x1209, "pid": 0x0001}]),
    )
    devs = kb.KillerBeeFeralRF.list_devices()
    assert devs and devs[0]["port"] == "/dev/ttyACM0"


def test_inject_with_delay_sleeps_between_frames(monkeypatch):
    fr = FakeRadio()
    a = _adapter(monkeypatch, fr)
    sleeps = []
    monkeypatch.setattr(kb.time, "sleep", lambda s: sleeps.append(s))
    a.inject(b"\x01\x08\x00\xff\xff\xff\xff", channel=11, count=3, delay=0.5)
    assert sleeps == [0.5, 0.5]
    assert len(fr.tx) == 3


def test_inject_oversize_raises(monkeypatch):
    a = _adapter(monkeypatch)
    with pytest.raises(Exception):
        a.inject(b"\x00" * 200)
