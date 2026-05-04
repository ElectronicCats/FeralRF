"""F17 — Sub-1GHz device personalities (S1-S3).

Each personality picks a preset from PROP_PRESETS + canonical payload.
W-MBus T1: preset wmbus_868_t1 is not yet in PROP_PRESETS (per F17
planning verification 2026-05-04); falls back to msk_868_50k which is
validated and matches W-MBus T1 modulation (MSK 868 MHz).
"""

import time
from dataclasses import dataclass

from feralrf.enums import PHY
from feralrf.presets import PROP_PRESETS
from feralrf.radio import Radio


@dataclass(frozen=True)
class Sub1GhzPersonality:
    """A Sub-1GHz device identity.

    Attributes:
        name: human-readable label.
        preset_name: key into PROP_PRESETS (caller verifies it exists).
        payload: raw frame bytes per device type.
    """

    name: str
    preset_name: str
    payload: bytes


# Generic GFSK 868 sensor: device_id (2) + temp (2) + humid (2) + checksum (2)
GFSK_868_SENSOR = Sub1GhzPersonality(
    name="Generic GFSK 868 sensor",
    preset_name="gfsk_868_50k",
    payload=bytes.fromhex(
        "aabb"  # device_id
        "0102"  # temp = 25.8 C scaled
        "0304"  # humid = 76.8% scaled
        "abcd"  # checksum
    ),
)


GFSK_433_SENSOR = Sub1GhzPersonality(
    name="Generic GFSK 433 sensor",
    preset_name="gfsk_433_50k",
    payload=bytes.fromhex(
        "ccdd"  # device_id
        "0506"  # temp
        "0708"  # humid
        "eeff"  # checksum
    ),
)


# W-MBus T1 meter: 6-byte preamble pattern + L-field + C-field + M-field +
# A-field (manufacturer + serial + version + device type) + canned payload.
# Preset fallback documented in module docstring.
WMBUS_T1_METER = Sub1GhzPersonality(
    name="W-MBus T1 meter",
    preset_name="msk_868_50k" if "msk_868_50k" in PROP_PRESETS else "gfsk_868_50k",
    payload=bytes.fromhex(
        "1e44"  # L=0x1E (30 bytes), C=0x44 (SND-NR)
        "2c2e"  # Mfg=ELS (Elster) LE
        "01020304"  # Serial 0x04030201 LE
        "0100"  # Version 0x01, Device type 0x00 (Other)
    )
    + bytes.fromhex("789aBCDE" * 4),  # 16 bytes canned payload
)


SUB1GHZ_PERSONALITIES = (GFSK_868_SENSOR, GFSK_433_SENSOR, WMBUS_T1_METER)


def emulate(
    radio: Radio,
    personality: Sub1GhzPersonality,
    count: int = 50,
    interval_ms: int = 100,
    power_dbm: int = 0,
) -> int:
    """TX `count` Sub-1GHz frames at `interval_ms`. Configures PHY+preset."""
    radio.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
    radio.configure_prop(**PROP_PRESETS[personality.preset_name])
    sent = 0
    for _ in range(count):
        radio.transmit(personality.payload, power_dbm=power_dbm)
        sent += 1
        if interval_ms > 0:
            time.sleep(interval_ms / 1000.0)
    return sent
