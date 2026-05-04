"""F17 — IEEE 802.15.4 device personalities.

Channels 11/15/20/25 (Zigbee/Thread typical). Frames PHY-level only —
firmware appends FCS automatically (per F12 BLE Scanner work). No MAC stack,
no encryption, no association.
"""

import time
from dataclasses import dataclass

from feralrf.enums import PHY
from feralrf.radio import Radio


@dataclass(frozen=True)
class Ieee154Personality:
    """An IEEE 802.15.4 device identity.

    Attributes:
        name: human-readable label.
        pan_id: 16-bit PAN ID.
        short_addr: 16-bit short address.
        channel: 802.15.4 channel (11/15/20/25 typical).
        payload: raw PHY-layer frame WITHOUT FCS (firmware adds it).
    """

    name: str
    pan_id: int
    short_addr: int
    channel: int
    payload: bytes


# Beacon-enabled coordinator: FCF=0x0080 (beacon, src 16-bit, dst none),
# seq=0x42, src_pan=0x1234 LE, src_addr=0x0001 LE,
# super_frame_spec=0xff0f LE (BO=15, SO=15, PAN coordinator),
# GTS spec=0x00, pending addr spec=0x00, beacon payload="HELLO".
_BEACON_FRAME = (
    bytes.fromhex(
        "0080"  # FCF: beacon frame, src 16-bit, no security, no ack req
        "42"  # seq
        "3412"  # src PAN ID 0x1234 LE
        "0100"  # src short addr 0x0001 LE
        "ff0f"  # superframe spec: BO=15, SO=15, PAN coord, no association
        "00"  # GTS spec: 0 GTS
        "00"  # Pending addr spec: 0
    )
    + b"HELLO"
)

BEACON_COORDINATOR = Ieee154Personality(
    name="Beacon-enabled coordinator",
    pan_id=0x1234,
    short_addr=0x0001,
    channel=15,
    payload=_BEACON_FRAME,
)


# Data-poll end device: FCF=0x6188 (data frame, src+dst 16-bit, ack req),
# seq=0x43, dst_pan=0x1234 LE, dst_addr=0x0001 LE (coordinator),
# src_addr=0x0042 LE (this device), payload="DATA".
_DATA_FRAME = (
    bytes.fromhex(
        "6188"  # FCF: data frame, dst+src 16-bit, ack req, no security
        "43"  # seq
        "3412"  # dst PAN ID 0x1234 LE
        "0100"  # dst short addr 0x0001 LE (coord)
        "4200"  # src short addr 0x0042 LE
    )
    + b"DATA"
)

DATA_POLL_END_DEVICE = Ieee154Personality(
    name="Data-poll end device",
    pan_id=0x1234,
    short_addr=0x0042,
    channel=15,
    payload=_DATA_FRAME,
)


IEEE154_PERSONALITIES = (BEACON_COORDINATOR, DATA_POLL_END_DEVICE)


def emulate(
    radio: Radio,
    personality: Ieee154Personality,
    count: int = 50,
    interval_ms: int = 100,
    power_dbm: int = 0,
) -> int:
    """TX `count` IEEE154 frames at `interval_ms`. Configures PHY+channel."""
    radio.set_phy(PHY.IEEE_802_15_4, channel=personality.channel)
    sent = 0
    for _ in range(count):
        radio.transmit(personality.payload, power_dbm=power_dbm)
        sent += 1
        if interval_ms > 0:
            time.sleep(interval_ms / 1000.0)
    return sent
