"""Expose a CatSniffer (via feralrf.Radio) as a KillerBee IEEE 802.15.4 device.

`killerbee` is an OPTIONAL dependency. It is imported lazily via _kbcaps()
so importing feralrf never requires killerbee to be installed.
"""

import time
from datetime import datetime, timezone
from typing import Optional

from feralrf.enums import PHY
from feralrf.radio import Radio


def _kbcaps():
    """Return the killerbee KBCapabilities class (lazy optional import)."""
    from killerbee.kbutils import KBCapabilities

    return KBCapabilities


class KillerBeeFeralRF:
    NAME = "FeralRF CatSniffer (CC1352)"

    @staticmethod
    def list_devices():
        return Radio.list_devices()

    def __init__(self, dev, radio: Optional[Radio] = None, reset_on_init: bool = False):
        self.dev = dev
        self.radio = radio if radio is not None else Radio(port=dev)
        # Opt-in power-cycle before selecting the IEEE PHY. Rationale: a stale PHY
        # state (e.g. the stick left in BLE mode) can make the first IEEE session
        # misbehave (FeralRF f9-partial: IEEE<->BLE needs a reset between modes).
        # DEFAULT OFF: on real hardware (2026-07-02 bench, stock RP2040 passthrough)
        # the reset_device() boot/exit cycle disrupts the CC1352 and the following
        # init() times out — verified that a plain init works reliably while the
        # reset breaks it. Enable only where the reset is safe on your bridge firmware, or
        # power-cycle the stick manually if you need a guaranteed-clean PHY.
        if reset_on_init:
            try:
                self.radio.connect()
                self.radio.reset_device()
            except Exception:
                pass
        self._info = self.radio.init()
        self.dev = self.radio.port or dev
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

    def set_channel(self, channel, page=0):
        if not (11 <= channel <= 26):
            raise ValueError("channel %r out of range 11-26" % channel)
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
            0: pkt.data,
            1: pkt.crc_ok,
            2: pkt.rssi_dbm,
            "bytes": pkt.data,
            "validcrc": pkt.crc_ok,
            "rssi": pkt.rssi_dbm,
            "dbm": pkt.rssi_dbm,
            "location": None,
            "datetime": datetime.now(timezone.utc),
        }

    def inject(self, packet, channel=None, count=1, delay=0, page=0):
        if channel is not None:
            self.set_channel(channel, page)
        mpdu = packet[:-2] if len(packet) >= 5 else packet  # RF core appends FCS
        if len(mpdu) > 125:
            raise ValueError("frame too long (%d > 125)" % len(mpdu))
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
