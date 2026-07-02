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
            0: pkt.data,
            1: pkt.crc_ok,
            2: pkt.rssi_dbm,
            "bytes": pkt.data,
            "validcrc": pkt.crc_ok,
            "rssi": pkt.rssi_dbm,
            "dbm": pkt.rssi_dbm,
            "location": None,
            "datetime": datetime.utcnow(),
        }
