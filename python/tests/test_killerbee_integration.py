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
