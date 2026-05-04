"""F17 — BLE peripheral personalities (advertising-only, B1).

PHY-level emulation of common BLE devices. Wraps attacks.ble.adv_spoof for
the actual TX path. NO connection acceptance (that requires F20+F21).
"""

from dataclasses import dataclass

from feralrf import attacks
from feralrf.radio import Radio


@dataclass(frozen=True)
class BlePersonality:
    """A BLE device identity defined by its advertising signature.

    Attributes:
        name: human-readable label.
        target_mac: MAC address string "AA:BB:CC:DD:EE:FF" (per adv_spoof API).
        advertising_payload: raw advertising bytes (post-PDU-header AD records).
    """

    name: str
    target_mac: str
    advertising_payload: bytes


# Soundcore Boom 2 — pinned in test_emulation.py (F13 retro-fill).
# Fast Pair Model ID 0x8F95F8.
_SOUNDCORE_ANKER_BLOCK = bytes.fromhex("02010a0505daf57b010ffff42b7d355a0e0000000000000000")
_SOUNDCORE_FASTPAIR = bytes.fromhex("020af606162cfe8f95f8")

SOUNDCORE_BOOM_2 = BlePersonality(
    name="Soundcore Boom 2",
    target_mac="CB:2B:7D:35:5A:0E",
    advertising_payload=_SOUNDCORE_ANKER_BLOCK + _SOUNDCORE_FASTPAIR,
)


# Apple AirPods Pro — Apple Manufacturer Specific (Mfg ID 0x004C) +
# Continuity protocol Proximity Pairing message (sub-type 0x07 for AirPods).
APPLE_AIRPODS_PRO = BlePersonality(
    name="Apple AirPods Pro",
    target_mac="DE:AD:BE:EF:CA:FE",
    advertising_payload=bytes.fromhex(
        "02010a"  # Flags AD: LE General Discoverable + BR/EDR Not Supported
        "1bff4c00"  # Mfg Specific Data: len=0x1B, type=0xFF, Mfg ID=0x004C LE
        "0719"  # Sub-type=0x07 (Proximity Pairing) + len=0x19
        "0220"  # AirPods Pro model bytes
        "75aa30010000000000000000000000000000000000"  # padding+status (canonical)
    ),
)


# Google Fast Pair generic — Service Data UUID 0xFE2C + 3-byte model ID.
# Default model_id 0x2C01A2 (test value, no real association).
GOOGLE_FASTPAIR_GENERIC = BlePersonality(
    name="Google Fast Pair generic",
    target_mac="DE:AD:BE:EF:CA:FF",
    advertising_payload=bytes.fromhex(
        "02010a"  # Flags AD
        "06"  # AD len=6
        "16"  # AD type=0x16 (Service Data 16-bit UUID)
        "2cfe"  # UUID 0xFE2C LE
        "2c01a2"  # Model ID 0x2C01A2 (3 bytes BE)
    ),
)


BLE_PERSONALITIES = (
    SOUNDCORE_BOOM_2,
    APPLE_AIRPODS_PRO,
    GOOGLE_FASTPAIR_GENERIC,
)


def emulate(
    radio: Radio,
    personality: BlePersonality,
    count: int = 50,
    interval_ms: int = 100,
    channel: int = 37,
    power_dbm: int = 0,
) -> int:
    """Emit `count` ADV_NONCONN_IND PDUs at `interval_ms` per personality.

    Reuses attacks.ble.adv_spoof which handles set_phy + set_ble_addr + transmit.
    Returns the count successfully sent (== count if no exception raised).
    """
    interval_us = max(0, interval_ms * 1000)
    result = attacks.ble.adv_spoof(
        radio,
        target_addr=personality.target_mac,
        adv_data=personality.advertising_payload,
        count=count,
        channel=channel,
        power_dbm=power_dbm,
        interval_us=interval_us,
    )
    return int(result.get("total_sent", count))
