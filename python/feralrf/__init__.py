"""
FeralRF - Python API for CatSniffer RF pentesting

Supports: BLE, IEEE 802.15.4 (Zigbee/Thread), Sub-1GHz, OOK, GFSK/FSK

Public API status:
- Stable: session control, RX/TX multi-PHY, proprietary configuration,
  BLE scan configuration, OOK recovery.
- Experimental: jamming helpers.
- Pending: spectrum helpers, GATT/initiator workflows.
"""

from feralrf.enums import (
    EXPERIMENTAL_COMMANDS,
    PENDING_COMMAND_IDS,
    PHY,
    STABLE_COMMANDS,
    Command,
    Response,
)
from feralrf.exceptions import CommandError, ConnectionError, FeralRFError, ProtocolError
from feralrf.presets import PROP_PRESETS
from feralrf.radio import (
    ConnectionResult,
    ConnectionStatus,
    DeviceInfo,
    DeviceStats,
    GattCharacteristic,
    GattDiscoveryResult,
    GattService,
    Packet,
    Radio,
)

__version__ = "0.2.0"
__author__ = "Electronic Cats"

__all__ = [
    "Radio",
    "Packet",
    "DeviceInfo",
    "DeviceStats",
    "ConnectionResult",
    "ConnectionStatus",
    "GattService",
    "GattCharacteristic",
    "GattDiscoveryResult",
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
]
