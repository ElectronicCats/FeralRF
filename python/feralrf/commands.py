"""
FeralRF - Command builders
"""

import struct

from feralrf.enums import Command


class CommandBuilder:
    """Build command payloads"""

    @staticmethod
    def radio_init() -> bytes:
        """Initialize radio subsystem"""
        return bytes([Command.RADIO_INIT])

    @staticmethod
    def set_channel(channel: int) -> bytes:
        """Set RF channel"""
        return bytes([Command.SET_CHANNEL, channel])

    @staticmethod
    def set_power(power_dbm: int) -> bytes:
        """Set TX power in dBm"""
        return bytes([Command.SET_POWER, power_dbm & 0xFF])

    @staticmethod
    def set_phy(phy: int, channel: int = 0, frequency_hz: int = 0) -> bytes:
        """Set PHY type and channel"""
        return struct.pack("<BBHI", Command.SET_PHY, phy, channel, frequency_hz)

    @staticmethod
    def get_info() -> bytes:
        """Get device info"""
        return bytes([Command.GET_INFO])

    @staticmethod
    def rx_start() -> bytes:
        """Start receiving"""
        return bytes([Command.RX_START])

    @staticmethod
    def rx_stop() -> bytes:
        """Stop receiving"""
        return bytes([Command.RX_STOP])

    @staticmethod
    def rx_set_promiscuous(enable: bool) -> bytes:
        """Set promiscuous mode"""
        return bytes([Command.RX_SET_PROMISCUOUS, 1 if enable else 0])

    @staticmethod
    def tx_raw(packet: bytes, power_dbm: int = -128) -> bytes:
        """Transmit raw packet"""
        length = len(packet)
        return bytes([Command.TX_RAW, length]) + packet + bytes([power_dbm & 0xFF])

    @staticmethod
    def jam_continuous(channel: int, power_dbm: int = 0) -> bytes:
        """Start continuous wave jamming"""
        return bytes([Command.JAM_CONTINUOUS, channel, power_dbm & 0xFF])

    @staticmethod
    def jam_stop() -> bytes:
        """Stop jamming"""
        return bytes([Command.JAM_STOP])

    @staticmethod
    def spectrum_scan(
        start_freq_hz: int,
        end_freq_hz: int,
        step_khz: int = 1000,
        samples: int = 10,
        dwell_ms: int = 10,
    ) -> bytes:
        """Start spectrum scan"""
        return struct.pack(
            "<BIIHBB",
            Command.SPECTRUM_SCAN,
            start_freq_hz,
            end_freq_hz,
            step_khz,
            samples,
            dwell_ms,
        )

    @staticmethod
    def spectrum_stop() -> bytes:
        """Stop spectrum scan"""
        return bytes([Command.SPECTRUM_STOP])
