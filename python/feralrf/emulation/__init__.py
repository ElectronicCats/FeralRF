"""F17 — Device emulation. PHY-level personalities for BLE / IEEE154 / Sub-1GHz / OOK.

NOT a stack emulator (no auth, no framing, no encryption). Each personality
transmits canonical payload bytes via burst (count + interval_ms) so a sniffer
on a separate board can detect the device signature. See:
  - docs/superpowers/specs/2026-05-04-f17-device-emulation-design.md
"""

from feralrf.emulation.ble_peripheral import (
    APPLE_AIRPODS_PRO,
    BLE_PERSONALITIES,
    GOOGLE_FASTPAIR_GENERIC,
    SOUNDCORE_BOOM_2,
    BlePersonality,
)
from feralrf.emulation.ble_peripheral import emulate as emulate_ble

__all__ = [
    "BlePersonality",
    "SOUNDCORE_BOOM_2",
    "APPLE_AIRPODS_PRO",
    "GOOGLE_FASTPAIR_GENERIC",
    "BLE_PERSONALITIES",
    "emulate_ble",
]
