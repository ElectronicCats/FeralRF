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
    def set_adv_hop(enabled: bool) -> bytes:
        """Payload for SET_ADV_HOP"""
        return bytes([1 if enabled else 0])

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
    def tx_frame(packet: bytes) -> bytes:
        """Payload for TX_FRAME"""
        length = len(packet)
        return bytes([length & 0xFF]) + packet

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
    def tx_continuous(packet: bytes, interval_us: int) -> bytes:
        """Payload for TX_CONTINUOUS"""
        length = len(packet)
        return bytes([length & 0xFF]) + packet + struct.pack("<I", interval_us & 0xFFFFFFFF)

    @staticmethod
    def tx_stop() -> bytes:
        """No payload for TX_STOP"""
        return b""

    @staticmethod
    def jam_continuous(channel: int, power_dbm: int = 20, duration_ms: int = 3000) -> bytes:
        """Payload for JAM_CONTINUOUS"""
        return bytes([channel & 0xFF, power_dbm & 0xFF]) + struct.pack("<H", duration_ms & 0xFFFF)

    @staticmethod
    def jam_stop() -> bytes:
        """No payload for JAM_STOP"""
        return b""

    @staticmethod
    def set_ble_addr(addr: bytes) -> bytes:
        """Payload for SET_BLE_ADDR (6 bytes, little-endian)"""
        if len(addr) != 6:
            raise ValueError("BLE address must be 6 bytes")
        return addr

    @staticmethod
    def set_prop_config(
        frequency_hz: int,
        mod_type: int = 1,
        symbol_rate: int = 50000,
        deviation: int = 100,
        rx_bw: int = 0x52,
        sync_word: int = 0x930B51DE,
        format_conf: int = 0,
    ) -> bytes:
        """Payload for SET_PROP_CONFIG (18 bytes):
        freq_hz(4) | mod_type(1) | symbol_rate(4) | deviation(2) | rx_bw(1) | sync_word(4) |
        format_conf(2)
        """
        return struct.pack(
            "<IBIHBIH",
            frequency_hz & 0xFFFFFFFF,
            mod_type & 0xFF,
            symbol_rate & 0xFFFFFFFF,
            deviation & 0xFFFF,
            rx_bw & 0xFF,
            sync_word & 0xFFFFFFFF,
            format_conf & 0xFFFF,
        )

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

    @staticmethod
    def ble_connect(addr_le: bytes, addr_type: int) -> bytes:
        """Payload for CMD_CONNECT: 6-byte LE address + 1-byte address type.

        Args:
            addr_le: Peer address in little-endian wire order
                (reversed of AA:BB:CC:DD:EE:FF).
            addr_type: 0 for public, 1 for random.
        """
        if len(addr_le) != 6:
            raise ValueError("addr_le must be exactly 6 bytes")
        return bytes(addr_le) + bytes([addr_type & 0xFF])

    @staticmethod
    def ble_disconnect() -> bytes:
        """No payload for CMD_DISCONNECT."""
        return b""

    @staticmethod
    def conn_status() -> bytes:
        """No payload for CMD_CONN_STATUS."""
        return b""

    @staticmethod
    def gatt_discover() -> bytes:
        """No payload for CMD_GATT_DISCOVER."""
        return b""

    @staticmethod
    def gatt_read(handle: int) -> bytes:
        """Payload for CMD_GATT_READ: 2-byte LE attribute handle."""
        return struct.pack("<H", handle & 0xFFFF)

    @staticmethod
    def debug_timing() -> bytes:
        """No payload for CMD_DEBUG_TIMING."""
        return b""

    @staticmethod
    def debug_conn_params() -> bytes:
        """No payload for CMD_DEBUG_CONN_PARAMS."""
        return b""

    @staticmethod
    def gatt_write(handle: int, data: bytes) -> bytes:
        """Payload for CMD_GATT_WRITE: 2-byte LE handle + value bytes."""
        return struct.pack("<H", handle & 0xFFFF) + bytes(data)

    @staticmethod
    def gatt_subscribe(handle: int, enable: bool, indicate: bool = False) -> bytes:
        """Build CMD_GATT_SUBSCRIBE payload: handle_le[2] + enable[1] + indicate[1]."""
        return struct.pack("<H", handle & 0xFFFF) + bytes(
            [1 if enable else 0, 1 if indicate else 0]
        )

    @staticmethod
    def gatt_exchange_mtu(client_mtu: int = 23) -> bytes:
        """Payload for CMD_GATT_EXCHANGE_MTU: 2-byte LE client MTU.

        Args:
            client_mtu: MTU we advertise to the peer. Per BT Core Spec
                Vol 3 Part F §3.2.8, ATT MTU MUST be >= 23.
        """
        if client_mtu < 23 or client_mtu > 0xFFFF:
            raise ValueError(f"client_mtu must be in [23, 65535], got {client_mtu}")
        return struct.pack("<H", client_mtu & 0xFFFF)

    @staticmethod
    def gatt_read_by_uuid(start: int, end: int, uuid: int) -> bytes:
        """Payload for CMD_GATT_READ_BY_UUID: start[2] + end[2] + uuid16[2].

        Only 16-bit UUIDs are supported in this revision (matches firmware
        send_read_by_type_req). 128-bit UUID support deferred.
        """
        if start < 1 or start > 0xFFFF:
            raise ValueError(f"start handle out of range: {start}")
        if end < start or end > 0xFFFF:
            raise ValueError(f"end ({end}) must be >= start ({start}) and <= 0xFFFF")
        return struct.pack("<HHH", start & 0xFFFF, end & 0xFFFF, uuid & 0xFFFF)

    @staticmethod
    def follow_start(target_mac_le: "bytes | None" = None) -> bytes:
        """Build CMD_FOLLOW_START payload.

        Args:
            target_mac_le: 6-byte MAC in BLE little-endian (LSB first), or
                None for wildcard (capture any CONNECT_IND seen).

        Wire format: [target_mac_le:6]  (all-zero ⇒ wildcard).
        """
        if target_mac_le is None:
            return b"\x00" * 6
        if len(target_mac_le) != 6:
            raise ValueError(f"target_mac_le must be exactly 6 bytes, got {len(target_mac_le)}")
        return bytes(target_mac_le)

    @staticmethod
    def follow_stop() -> bytes:
        """Build CMD_FOLLOW_STOP payload (empty)."""
        return b""
