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

    @classmethod
    def parse(cls, payload: bytes) -> 'RxPacketResponse':
        if len(payload) < 13:
            raise ValueError("Payload too short for RX packet")

        timestamp = struct.unpack('<Q', payload[0:8])[0]
        channel = payload[8]
        rssi = struct.unpack('<b', bytes([payload[9]]))[0]
        lqi = payload[10]
        crc_ok = payload[11] == 1
        pkt_len = payload[12]
        data = payload[13:13 + pkt_len]

        return cls(
            timestamp_us=timestamp,
            channel=channel,
            rssi_dbm=rssi,
            lqi=lqi,
            crc_ok=crc_ok,
            data=data,
        )


@dataclass
class SpectrumDataResponse:
    """Spectrum data response payload"""
    frequency_hz: int
    rssi_dbm: int
    samples: list

    @classmethod
    def parse(cls, payload: bytes) -> 'SpectrumDataResponse':
        if len(payload) < 5:
            raise ValueError("Payload too short for spectrum data")

        freq = struct.unpack('<I', payload[0:4])[0]
        rssi = struct.unpack('<b', bytes([payload[4]]))[0]
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
    def parse(cls, payload: bytes) -> 'InfoResponse':
        if len(payload) < 4:
            raise ValueError("Payload too short for info")

        return cls(
            firmware_major=payload[0],
            firmware_minor=payload[1],
            firmware_patch=payload[2],
            capabilities=payload[3] if len(payload) > 3 else 0,
            serial=payload[4:12].hex() if len(payload) > 4 else "",
        )
