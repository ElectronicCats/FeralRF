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
