"""End-to-end integration test: CatSniffer driven through the real KillerBee.

Skipped automatically where `killerbee` is not installed (e.g. CI), so it does
not affect the core feralrf suite. When killerbee IS present, it verifies the
adapter against the REAL KBCapabilities, the dev_feralcat shim, the
kbutils.iscatsniffer/devlist detection hooks, and that KillerBee's own
dispatch constructs our driver.
"""

import pytest

kb = pytest.importorskip("killerbee")

from killerbee.kbutils import KBCapabilities  # noqa: E402

from feralrf.integrations import killerbee as kbint  # noqa: E402
from feralrf.radio import DeviceInfo  # noqa: E402


class FakeRadio:
    """Stands in for feralrf.Radio so no real serial port is opened."""

    def __init__(self, port=None, baudrate=921600):
        self.port = port
        self.channel = None

    def connect(self):
        pass

    def reset_device(self, wait=1.5):
        pass

    def init(self):
        return DeviceInfo(firmware_version="2.0.0", capabilities=0x01, serial="00" * 8)

    def set_phy(self, phy, channel=0, frequency_hz=0):
        self.channel = channel

    def set_channel(self, ch):
        self.channel = ch

    def start_rx(self):
        pass

    def stop_rx(self):
        pass

    def read_one_packet(self, timeout=1.0):
        return None

    def transmit_frame(self, packet, timeout=5.0):
        pass

    def start_jam(self, channel, power_dbm=20, duration_ms=3000, timeout=5.0):
        pass

    def stop_jam(self, timeout=5.0):
        pass

    def disconnect(self):
        pass


def test_adapter_uses_real_kbcapabilities():
    a = kbint.KillerBeeFeralRF(dev="/dev/fake", radio=FakeRadio(port="/dev/fake"))
    assert a.check_capability(KBCapabilities.SNIFF)
    assert a.check_capability(KBCapabilities.SETCHAN)
    assert a.check_capability(KBCapabilities.INJECT)
    assert a.check_capability(KBCapabilities.PHYJAM)
    assert a.check_capability(KBCapabilities.FREQ_2400)
    caps = a.get_capabilities()  # real KBCapabilities.getlist() -> dict
    assert caps[KBCapabilities.INJECT] is True
    # get_dev_info reports the (auto-detected) port + firmware
    info = a.get_dev_info()
    assert info[0] == "/dev/fake" and info[2] == "2.0.0"


def test_shim_reexports_adapter():
    from killerbee.dev_feralcat import FERALCAT

    assert FERALCAT is kbint.KillerBeeFeralRF


def test_iscatsniffer_and_devlist(monkeypatch):
    from killerbee import kbutils

    monkeypatch.setattr(
        "feralrf.radio.Radio.list_devices",
        staticmethod(
            lambda: [{"port": "/dev/ttyACM0", "vid": 0x1209, "pid": 1, "description": "Cat-Bridge"}]
        ),
    )
    assert kbutils.iscatsniffer("/dev/ttyACM0") is True
    assert kbutils.iscatsniffer("/dev/ttyUSB9") is False

    # devlist must list it. Neutralize USB enumeration (no libusb backend here)
    # and pin the serial port list.
    monkeypatch.setattr(kbutils, "devlist_usb_v1x", lambda vendor=None, product=None: [])
    monkeypatch.setattr(kbutils, "get_serial_ports", lambda include=None: ["/dev/ttyACM0"])
    dl = kbutils.devlist()
    assert any(e[0] == "/dev/ttyACM0" and "CatSniffer" in e[1] for e in dl)


def test_killerbee_forced_dispatch(monkeypatch):
    # Force hardware="feralcat"; swap Radio for the fake so init() opens nothing.
    monkeypatch.setattr(kbint, "Radio", FakeRadio)
    k = kb.KillerBee(device="/dev/ttyACM0", hardware="feralcat")
    assert isinstance(k.driver, kbint.KillerBeeFeralRF)
    assert k.driver.check_capability(KBCapabilities.INJECT)
