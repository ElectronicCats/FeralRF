"""F17 — OOK / ASK device personalities (O1-O3).

OOK locks the radio; emulate() always ends with radio.reset_device() so
subsequent non-OOK operations work without firmware reflash. See
project_ook_bug + feedback_workflow memories.

Payloads are CANONICAL bit patterns (no real reverse engineering of specific
remotes). Sniffer cross-validation only checks that bytes received match
the bytes transmitted — not that they decode as a "real" Hörmann frame.
"""

import time
from dataclasses import dataclass

from feralrf.enums import PHY
from feralrf.presets import PROP_PRESETS
from feralrf.radio import Radio


@dataclass(frozen=True)
class OokPersonality:
    """An OOK device identity.

    Attributes:
        name: human-readable label.
        preset_name: key into PROP_PRESETS (must be an OOK preset, mod_type=2).
        payload: raw bit-pattern bytes (canonical, not derived from real device).
    """

    name: str
    preset_name: str
    payload: bytes


# PT2262 generic 433 garage remote — 24-bit canonical pattern.
PT2262_GARAGE_433 = OokPersonality(
    name="PT2262 garage 433 MHz",
    preset_name="ook_433_4k8",
    payload=bytes.fromhex("a5a55a"),
)


# EV1527 sensor — 24-bit ID pattern, lower symbol rate.
EV1527_SENSOR_433 = OokPersonality(
    name="EV1527 wireless sensor 433 MHz",
    preset_name="ook_433_2k4",
    payload=bytes.fromhex("3c5aa9"),
)


# Hörmann garage 868 — 64-bit canonical frame.
HORMANN_GARAGE_868 = OokPersonality(
    name="Hörmann garage 868 MHz",
    preset_name="ook_868_4k8",
    payload=bytes.fromhex("0123456789abcdef"),
)


OOK_PERSONALITIES = (PT2262_GARAGE_433, EV1527_SENSOR_433, HORMANN_GARAGE_868)


def emulate(
    radio: Radio,
    personality: OokPersonality,
    count: int = 50,
    interval_ms: int = 100,
    power_dbm: int = 0,
    auto_reset: bool = True,
) -> int:
    """TX `count` OOK frames at `interval_ms`. Auto-resets device post-TX.

    auto_reset=True calls radio.reset_device() at the end so subsequent
    non-OOK operations work without firmware reflash. Set False if caller
    will handle reset explicitly (e.g. inside a smoke loop with multiple
    OOK personalities back-to-back).
    """
    radio.set_phy(PHY.PROPRIETARY_GFSK, channel=0)
    radio.configure_prop(**PROP_PRESETS[personality.preset_name])
    sent = 0
    try:
        for _ in range(count):
            radio.transmit(personality.payload, power_dbm=power_dbm)
            sent += 1
            if interval_ms > 0:
                time.sleep(interval_ms / 1000.0)
    finally:
        if auto_reset:
            radio.reset_device()
    return sent
