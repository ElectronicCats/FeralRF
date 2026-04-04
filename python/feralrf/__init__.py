"""
FeralRF - Python API for CatSniffer RF pentesting

Supports: BLE, IEEE 802.15.4 (Zigbee/Thread), Sub-1GHz, OOK, GFSK/FSK
"""

from feralrf.enums import PHY, Command, Response
from feralrf.exceptions import CommandError, ConnectionError, FeralRFError, ProtocolError
from feralrf.presets import PROP_PRESETS
from feralrf.radio import DeviceStats, Radio

__version__ = "0.2.0"
__author__ = "Electronic Cats"

__all__ = [
    "Radio",
    "DeviceStats",
    "PHY",
    "Command",
    "Response",
    "PROP_PRESETS",
    "FeralRFError",
    "ConnectionError",
    "ProtocolError",
    "CommandError",
]
