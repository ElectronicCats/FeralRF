"""
FeralRF - Jamming utilities

WARNING: RF jamming may be illegal in your jurisdiction.
Only use in authorized environments for research and testing.
"""

from dataclasses import dataclass
from enum import IntEnum
from typing import Optional


class JamType(IntEnum):
    """Jamming types"""
    CW = 0        # Continuous wave
    PATTERN = 1   # Custom pattern
    PACKET = 2    # Packet-based


class TriggerType(IntEnum):
    """Reactive jamming trigger types"""
    PREAMBLE = 0
    SYNC_WORD = 1
    ADDRESS_MATCH = 2


@dataclass
class JammingPolicy:
    """Jamming policy configuration"""

    trigger: TriggerType = TriggerType.PREAMBLE
    trigger_value: bytes = b''
    trigger_mask: bytes = b''
    jam_duration_us: int = 500
    jam_type: JamType = JamType.CW
    jam_data: bytes = b''
    channel: int = 0
    power_dbm: int = 0

    def to_payload(self) -> bytes:
        """Convert to command payload"""
        payload = bytearray()
        payload.append(self.trigger)
        payload.extend(self.trigger_value[:8].ljust(8, b'\x00'))
        payload.extend(self.trigger_mask[:8].ljust(8, b'\x00'))
        payload.extend(self.jam_duration_us.to_bytes(2, 'little'))
        payload.append(self.jam_type)
        payload.extend(self.jam_data[:64])
        return bytes(payload)


# Regulatory warning
REGULATORY_WARNING = """
╔══════════════════════════════════════════════════════════════════╗
║                         WARNING                                   ║
║                                                                   ║
║  RF jamming is ILLEGAL in most jurisdictions.                    ║
║                                                                   ║
║  This functionality is provided for:                             ║
║  - Authorized security testing                                   ║
║  - Research and education                                        ║
║  - CTF competitions                                              ║
║                                                                   ║
║  Users are responsible for ensuring compliance with local laws.  ║
║                                                                   ║
╚══════════════════════════════════════════════════════════════════╝
"""


def show_regulatory_warning() -> None:
    """Display regulatory warning"""
    print(REGULATORY_WARNING)
