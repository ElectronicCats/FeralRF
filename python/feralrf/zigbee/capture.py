"""Internal adapters from host capture models to immutable raw captures."""

from typing import TYPE_CHECKING

from feralrf.zigbee.core import CaptureContext, RawCapture

if TYPE_CHECKING:
    from feralrf.radio import Packet


def capture_from_packet(packet: "Packet", context: CaptureContext) -> RawCapture:
    """Snapshot a packet and its caller-supplied capture context."""

    return RawCapture(
        data=packet.data,
        context=context,
        timestamp_us=packet.timestamp_us,
        channel=packet.channel,
        rssi_dbm=packet.rssi_dbm,
        lqi=packet.lqi,
        crc_ok=packet.crc_ok,
    )
