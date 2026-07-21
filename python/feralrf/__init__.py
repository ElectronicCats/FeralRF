"""
FeralRF - Python API for CatSniffer RF pentesting

Supports: IEEE 802.15.4 (Zigbee/Thread), Sub-1GHz, OOK, GFSK/FSK, plus raw
BLE-PHY capture (no BLE protocol stack; use Sniffle for BLE).

Public API status:
- Stable: session control, RX/TX multi-PHY, proprietary configuration, OOK recovery.
- Experimental: jamming helpers.
- Pending: spectrum helpers.
"""

from feralrf.enums import (
    EXPERIMENTAL_COMMANDS,
    PENDING_COMMAND_IDS,
    PHY,
    STABLE_COMMANDS,
    Command,
    Response,
)
from feralrf.exceptions import (
    CommandError,
    ConnectionError,
    CryptoError,
    FeralRFError,
    ProtocolError,
    RadioError,
)
from feralrf.presets import PROP_PRESETS
from feralrf.radio import (
    DeviceInfo,
    DeviceStats,
    Packet,
    Radio,
    RxStreamError,
)

__version__ = "0.3.0"
__author__ = "Electronic Cats"

__all__ = [
    "Radio",
    "Packet",
    "DeviceInfo",
    "DeviceStats",
    "RxStreamError",
    "PHY",
    "Command",
    "Response",
    "STABLE_COMMANDS",
    "EXPERIMENTAL_COMMANDS",
    "PENDING_COMMAND_IDS",
    "PROP_PRESETS",
    "FeralRFError",
    "ConnectionError",
    "ProtocolError",
    "CommandError",
    "RadioError",
    "CryptoError",
]
