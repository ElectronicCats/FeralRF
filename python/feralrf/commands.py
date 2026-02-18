"""
FeralRF - Command builders
"""

import struct


class CommandBuilder:
    """Build command payloads"""

    @staticmethod
    def radio_init() -> bytes:
        """No payload for RADIO_INIT"""
        return b""

    @staticmethod
    def set_channel(channel: int) -> bytes:
        """Payload for SET_CHANNEL"""
        return bytes([channel & 0xFF])

    @staticmethod
    def set_power(power_dbm: int) -> bytes:
        """Payload for SET_POWER"""
        return bytes([power_dbm & 0xFF])

    @staticmethod
    def set_phy(phy: int, channel: int = 0, frequency_hz: int = 0) -> bytes:
        """Payload for SET_PHY"""
        return struct.pack("<BHI", phy & 0xFF, channel & 0xFFFF, frequency_hz & 0xFFFFFFFF)

    @staticmethod
    def get_info() -> bytes:
        """No payload for GET_INFO"""
        return b""

    @staticmethod
    def rx_start() -> bytes:
        """No payload for RX_START"""
        return b""

    @staticmethod
    def rx_stop() -> bytes:
        """No payload for RX_STOP"""
        return b""

    @staticmethod
    def rx_set_promiscuous(enable: bool) -> bytes:
        """Payload for RX_SET_PROMISCUOUS"""
        return bytes([1 if enable else 0])

    @staticmethod
    def tx_raw(packet: bytes, power_dbm: int = -128) -> bytes:
        """Payload for TX_RAW"""
        length = len(packet)
        return bytes([length & 0xFF]) + packet + bytes([power_dbm & 0xFF])

    @staticmethod
    def tx_burst(packet: bytes, count: int, interval_us: int) -> bytes:
        """Payload for TX_BURST"""
        length = len(packet)
        return (
            bytes([length & 0xFF])
            + packet
            + struct.pack("<HI", count & 0xFFFF, interval_us & 0xFFFFFFFF)
        )

    @staticmethod
    def jam_continuous(channel: int, power_dbm: int = 0) -> bytes:
        """Payload for JAM_CONTINUOUS"""
        return bytes([channel & 0xFF, power_dbm & 0xFF])

    @staticmethod
    def jam_stop() -> bytes:
        """No payload for JAM_STOP"""
        return b""

    @staticmethod
    def spectrum_scan(
        start_freq_hz: int,
        end_freq_hz: int,
        step_khz: int = 1000,
        samples: int = 10,
        dwell_ms: int = 10,
    ) -> bytes:
        """Payload for SPECTRUM_SCAN"""
        return struct.pack(
            "<IIHBB",
            start_freq_hz & 0xFFFFFFFF,
            end_freq_hz & 0xFFFFFFFF,
            step_khz & 0xFFFF,
            samples & 0xFF,
            dwell_ms & 0xFF,
        )

    @staticmethod
    def spectrum_stop() -> bytes:
        """No payload for SPECTRUM_STOP"""
        return b""
