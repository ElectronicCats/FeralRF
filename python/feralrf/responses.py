"""
FeralRF - Response parsers
"""

import struct
from dataclasses import dataclass
from typing import Optional


@dataclass
class RxPacketResponse:
    """RX packet response payload"""

    timestamp_us: int
    channel: int
    rssi_dbm: int
    lqi: int
    crc_ok: bool
    data: bytes
    ll_pdu_kind: Optional[int] = None
    ll_pdu_type: Optional[int] = None
    ll_pdu_flags: Optional[int] = None

    @classmethod
    def parse(cls, payload: bytes) -> "RxPacketResponse":
        if len(payload) < 13:
            raise ValueError("Payload too short for RX packet")

        timestamp = struct.unpack("<Q", payload[0:8])[0]
        channel = payload[8]
        rssi = struct.unpack("<b", bytes([payload[9]]))[0]
        lqi = payload[10]
        crc_ok = payload[11] == 1
        pkt_len = payload[12]
        data = payload[13 : 13 + pkt_len]
        ll_pdu_kind = None
        ll_pdu_type = None
        ll_pdu_flags = None
        ll_meta_offset = 13 + pkt_len

        if len(payload) >= ll_meta_offset + 2:
            ll_pdu_kind = payload[ll_meta_offset]
            ll_pdu_type = payload[ll_meta_offset + 1]
            if len(payload) >= ll_meta_offset + 3:
                ll_pdu_flags = payload[ll_meta_offset + 2]

        return cls(
            timestamp_us=timestamp,
            channel=channel,
            rssi_dbm=rssi,
            lqi=lqi,
            crc_ok=crc_ok,
            data=data,
            ll_pdu_kind=ll_pdu_kind,
            ll_pdu_type=ll_pdu_type,
            ll_pdu_flags=ll_pdu_flags,
        )


@dataclass
class SpectrumDataResponse:
    """Spectrum data response payload"""

    frequency_hz: int
    rssi_dbm: int
    samples: list

    @classmethod
    def parse(cls, payload: bytes) -> "SpectrumDataResponse":
        if len(payload) < 5:
            raise ValueError("Payload too short for spectrum data")

        freq = struct.unpack("<I", payload[0:4])[0]
        rssi = struct.unpack("<b", bytes([payload[4]]))[0]
        samples = list(payload[5:])

        return cls(
            frequency_hz=freq,
            rssi_dbm=rssi,
            samples=samples,
        )


@dataclass
class InfoResponse:
    """Device info response payload"""

    firmware_major: int
    firmware_minor: int
    firmware_patch: int
    capabilities: int
    serial: str

    @classmethod
    def parse(cls, payload: bytes) -> "InfoResponse":
        if len(payload) < 4:
            raise ValueError("Payload too short for info")

        return cls(
            firmware_major=payload[0],
            firmware_minor=payload[1],
            firmware_patch=payload[2],
            capabilities=payload[3] if len(payload) > 3 else 0,
            serial=payload[4:12].hex() if len(payload) > 4 else "",
        )
