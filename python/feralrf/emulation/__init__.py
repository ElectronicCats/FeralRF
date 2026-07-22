"""F17 — Device emulation. PHY-level personalities for IEEE154 / Sub-1GHz / OOK.

NOT a stack emulator (no auth, no framing, no encryption). Each personality
transmits canonical payload bytes via burst (count + interval_ms) so a sniffer
on a separate board can detect the device signature.
"""

from feralrf.emulation.ieee154_device import (
    BEACON_COORDINATOR,
    DATA_POLL_END_DEVICE,
    IEEE154_PERSONALITIES,
    Ieee154Personality,
)
from feralrf.emulation.ieee154_device import emulate as emulate_ieee154
from feralrf.emulation.ook_device import (
    EV1527_SENSOR_433,
    HORMANN_GARAGE_868,
    OOK_PERSONALITIES,
    PT2262_GARAGE_433,
    OokPersonality,
)
from feralrf.emulation.ook_device import emulate as emulate_ook
from feralrf.emulation.sub1ghz_device import (
    GFSK_433_SENSOR,
    GFSK_868_SENSOR,
    SUB1GHZ_PERSONALITIES,
    WMBUS_T1_METER,
    Sub1GhzPersonality,
)
from feralrf.emulation.sub1ghz_device import emulate as emulate_sub1ghz

__all__ = [
    "Ieee154Personality",
    "BEACON_COORDINATOR",
    "DATA_POLL_END_DEVICE",
    "IEEE154_PERSONALITIES",
    "emulate_ieee154",
    "Sub1GhzPersonality",
    "GFSK_868_SENSOR",
    "GFSK_433_SENSOR",
    "WMBUS_T1_METER",
    "SUB1GHZ_PERSONALITIES",
    "emulate_sub1ghz",
    "OokPersonality",
    "PT2262_GARAGE_433",
    "EV1527_SENSOR_433",
    "HORMANN_GARAGE_868",
    "OOK_PERSONALITIES",
    "emulate_ook",
]
