"""
FeralRF - Radio interface
"""

import time
from dataclasses import dataclass
from typing import Iterator, Optional, Set

import serial
import serial.tools.list_ports

from feralrf.commands import CommandBuilder
from feralrf.enums import PHY, Command, Response
from feralrf.exceptions import CommandError, ConnectionError, ProtocolError, TimeoutError
from feralrf.protocol import build_frame, cobs_decode, parse_frame


@dataclass
class Packet:
    """Received packet data"""

    timestamp_us: int
    channel: int
    rssi_dbm: int
    lqi: int
    crc_ok: bool
    data: bytes
    ll_pdu_kind: Optional[int] = None
    ll_pdu_type: Optional[int] = None
    ll_pdu_flags: Optional[int] = None


@dataclass
class DeviceInfo:
    """Device information"""

    firmware_version: str
    capabilities: int
    serial: str


@dataclass
class DeviceStats:
    """Device RX metrics"""

    rx_ok: int
    rx_crc_err: int
    rx_drop: int
    rx_overflow: int
    ll_kind_unknown: Optional[int] = None
    ll_kind_adv: Optional[int] = None
    ll_kind_scan: Optional[int] = None
    ll_kind_connect: Optional[int] = None
    ll_kind_data: Optional[int] = None


class Radio:
    """
    Synchronous Radio interface for FeralRF.

    Public API status:
        Stable:
            init, set_phy, set_channel, set_power, start_rx, read_packets,
            stop_rx, transmit, transmit_frame, transmit_burst,
            transmit_continuous, stop_transmit, get_stats, configure_prop,
            set_ble_addr, set_ble_addr_str, set_ble_scan_mode, set_adv_hop,
            reset_device.
        Experimental:
            start_jam, stop_jam.
        Pending:
            spectrum helpers, GATT discovery, non-BLE attack helpers in host.
    """

    CAPABILITY_RX_STATS = 0x01
    CAPABILITY_LL_PDU_META = 0x02
    CAPABILITY_LL_STATS_EXT = 0x04
    STABLE_METHODS = (
        "init",
        "set_phy",
        "set_channel",
        "set_power",
        "start_rx",
        "read_packets",
        "stop_rx",
        "transmit",
        "transmit_frame",
        "transmit_burst",
        "transmit_continuous",
        "stop_transmit",
        "get_stats",
        "configure_prop",
        "set_ble_addr",
        "set_ble_addr_str",
        "set_ble_scan_mode",
        "set_adv_hop",
        "reset_device",
    )
    EXPERIMENTAL_METHODS = (
        "start_jam",
        "stop_jam",
    )
    PENDING_FEATURES = (
        "spectrum",
        "gatt_discovery",
        "initiator_mode",
    )

    def __init__(self, port: Optional[str] = None, baudrate: int = 921600):
        self.port = port
        self.baudrate = baudrate
        self._serial: Optional[serial.Serial] = None
        self._seq = 0
        self._last_seq = 0
        self._phy: Optional[PHY] = None
        self._channel: int = 0
        self._capabilities: int = 0
        self._rx_buffer = bytearray()

    @staticmethod
    def list_devices() -> list:
        """List available FeralRF devices"""
        devices = []
        for port in serial.tools.list_ports.comports():
            # CatSniffer typically shows as USB serial
            if port.vid in (0x2E8A, 0x2341, 0x1A86, 0x10C4):
                devices.append(
                    {
                        "port": port.device,
                        "vid": port.vid,
                        "pid": port.pid,
                        "description": port.description,
                    }
                )
        return devices

    def connect(self) -> None:
        """Connect to device"""
        if self._serial and self._serial.is_open:
            return

        if not self.port:
            devices = self.list_devices()
            if not devices:
                raise ConnectionError("No FeralRF device found")
            self.port = devices[0]["port"]

        try:
            self._serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1.0,
                write_timeout=1.0,
            )
            # Give USB CDC bridge time to settle after open to avoid losing
            # the very first command frame on some host/bridge combinations.
            time.sleep(1.0)
            self._serial.reset_input_buffer()
            self._serial.reset_output_buffer()
        except serial.SerialException as e:
            raise ConnectionError(f"Failed to connect to {self.port}: {e}")

    def _get_shell_port(self) -> str:
        """Derive RP2040 shell port from bridge port (offset +2)."""
        import re

        if self.port is None:
            raise ConnectionError("Cannot derive shell port without an active bridge port")

        m = re.search(r"(\d+)$", self.port)
        if m:
            base_num = int(m.group(1))
            return self.port[: m.start(1)] + str(base_num + 2)
        raise ConnectionError(f"Cannot derive shell port from {self.port}")

    def reset_device(self, wait: float = 1.5) -> None:
        """Power-cycle the CC1352 via RP2040 shell reset pin.

        Sends 'exit' command to RP2040 shell which resets CC1352 and
        returns to passthrough mode. After reset, re-initializes the
        radio. Useful for recovering from OOK mode lock.

        Args:
            wait: Seconds to wait after reset for CC1352 to boot.
        """
        shell_port = self._get_shell_port()

        # Close current serial to avoid conflicts
        was_connected = self._serial and self._serial.is_open
        if was_connected:
            try:
                if self._serial is not None:
                    self._serial.close()
            except Exception:
                pass

        # Send reset via shell port: boot (enters bootloader, resets CC1352)
        # then exit (returns to passthrough, resets CC1352 again with correct baud)
        try:
            shell = serial.Serial(shell_port, 115200, timeout=1.0, write_timeout=1.0)
            shell.write(b"boot\r\n")
            time.sleep(0.5)
            shell.write(b"exit\r\n")
            time.sleep(0.3)
            shell.close()
        except serial.SerialException as e:
            raise ConnectionError(f"Failed to send reset via {shell_port}: {e}")

        time.sleep(wait)

        # Reconnect and re-init
        if was_connected:
            self._serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1.0,
                write_timeout=1.0,
            )
            time.sleep(0.5)
            self._serial.reset_input_buffer()
            self._serial.reset_output_buffer()
            self._seq = 0
            self._rx_buffer = bytearray()
            self.init()

    def disconnect(self) -> None:
        """Disconnect from device"""
        if self._serial:
            self._serial.close()
            self._serial = None

    def _next_seq(self) -> int:
        """Get next sequence number"""
        seq = self._seq
        self._seq = (self._seq + 1) & 0xFF
        return seq

    def _send_command(self, cmd: Command, payload: bytes = b"") -> None:
        """Send a command to the device"""
        if not self._serial:
            raise ConnectionError("Not connected")

        self._last_seq = self._next_seq()
        frame = build_frame(cmd, self._last_seq, payload)
        self._serial.write(frame)
        self._serial.flush()

    @staticmethod
    def _is_response_cmd(cmd_id: int) -> bool:
        """Responses occupy 0x80-0xFF range; commands are below 0x80."""
        return cmd_id >= 0x80

    def _read_response(
        self, timeout: Optional[float] = 1.0, expected: Optional[Set[int]] = None
    ) -> tuple:
        """Read and parse a response"""
        if not self._serial:
            raise ConnectionError("Not connected")

        deadline = None if timeout is None else (time.monotonic() + max(timeout, 0.0))
        ignored_command_frames = 0
        ignored_unexpected_responses = 0
        last_unexpected_response = None

        # Read until we get a complete frame (0x00 delimiter)
        while True:
            if deadline is not None:
                remaining = deadline - time.monotonic()
                if remaining <= 0.0:
                    details = []
                    if ignored_command_frames:
                        details.append(f"ignored {ignored_command_frames} command-frame echoes")
                    if ignored_unexpected_responses and last_unexpected_response is not None:
                        details.append(
                            f"ignored {ignored_unexpected_responses} unexpected response(s), "
                            f"last=0x{last_unexpected_response:02X}"
                        )
                    if details:
                        raise TimeoutError(f"Response timeout ({'; '.join(details)})")
                    raise TimeoutError("Response timeout")
                # Bound each serial read wait so we can enforce absolute deadline.
                self._serial.timeout = min(remaining, 0.1)
            else:
                self._serial.timeout = None

            byte = self._serial.read(1)
            if not byte:
                if deadline is not None and time.monotonic() < deadline:
                    # Keep waiting until global deadline expires.
                    continue
                details = []
                if ignored_command_frames:
                    details.append(f"ignored {ignored_command_frames} command-frame echoes")
                if ignored_unexpected_responses and last_unexpected_response is not None:
                    details.append(
                        f"ignored {ignored_unexpected_responses} unexpected response(s), "
                        f"last=0x{last_unexpected_response:02X}"
                    )
                if details:
                    raise TimeoutError(f"Response timeout ({'; '.join(details)})")
                raise TimeoutError("Response timeout")

            self._rx_buffer.extend(byte)

            if byte == b"\x00" and len(self._rx_buffer) > 1:
                # Try to decode frame
                try:
                    decoded = cobs_decode(bytes(self._rx_buffer))
                    self._rx_buffer.clear()
                    cmd_id, seq, payload = parse_frame(decoded)

                    # Ignore echoed command frames or stray command traffic.
                    if not self._is_response_cmd(cmd_id):
                        ignored_command_frames += 1
                        continue

                    # Skip async errors (seq=0xFF) — log but don't consume as response.
                    if seq == 0xFF:
                        import warnings

                        err_code = payload[0] if payload else 0
                        warnings.warn(f"Async RF error: code=0x{err_code:02X}", stacklevel=2)
                        continue

                    # Skip stale responses with mismatched seq — only when
                    # expecting a command response (not during RX stream).
                    if expected is not None and seq != self._last_seq:
                        ignored_unexpected_responses += 1
                        last_unexpected_response = cmd_id
                        continue

                    # If caller expects specific response types, keep reading until match.
                    if expected is not None and cmd_id not in expected:
                        ignored_unexpected_responses += 1
                        last_unexpected_response = cmd_id
                        continue

                    return cmd_id, seq, payload
                except Exception:
                    # Invalid/partial frame, continue reading next delimited frame.
                    self._rx_buffer.clear()
                    continue

    def init(self) -> DeviceInfo:
        """Initialize radio and get device info"""
        last_timeout: Optional[TimeoutError] = None
        attempts = 3

        for attempt in range(attempts):
            if attempt > 0:
                # Re-open the serial link on retry to recover from bad bridge state.
                self.disconnect()
                time.sleep(0.2)
            self.connect()

            self._send_command(Command.RADIO_INIT)

            try:
                # First init exchange can be delayed right after serial open.
                cmd_id, seq, payload = self._read_response(
                    timeout=2.5, expected={Response.ACK, Response.ERROR}
                )
            except TimeoutError as exc:
                last_timeout = exc
                if self._serial is not None:
                    self._serial.reset_input_buffer()
                if attempt < attempts - 1:
                    time.sleep(0.4 + (0.4 * attempt))
                    continue
                raise

            if cmd_id == Response.ERROR:
                raise CommandError("Radio init failed", payload[0] if payload else 0)
            if cmd_id == Response.ACK:
                break
            raise ProtocolError(f"Unexpected response to RADIO_INIT: 0x{cmd_id:02X}")
        else:
            if last_timeout is not None:
                raise last_timeout
            raise TimeoutError("Response timeout")

        # Get device info
        self._send_command(Command.GET_INFO)
        cmd_id, seq, payload = self._read_response(expected={Response.INFO, Response.ERROR})
        if cmd_id == Response.ERROR:
            raise CommandError("Get info failed", payload[0] if payload else 0)
        if cmd_id != Response.INFO:
            raise ProtocolError(f"Unexpected response to GET_INFO: 0x{cmd_id:02X}")
        if len(payload) < 12:
            raise ProtocolError(f"INFO payload too short: {len(payload)}")

        info = DeviceInfo(
            firmware_version=f"{payload[0]}.{payload[1]}.{payload[2]}",
            capabilities=payload[3],
            serial=payload[4:12].hex(),
        )
        self._capabilities = info.capabilities
        return info

    def set_phy(self, phy: PHY, channel: int = 0, frequency_hz: int = 0) -> None:
        """Set PHY type, channel, and optionally frequency in Hz"""
        self._send_command(Command.SET_PHY, CommandBuilder.set_phy(phy, channel, frequency_hz))
        cmd_id, seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})

        if cmd_id == Response.ERROR:
            raise CommandError("Set PHY failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to SET_PHY: 0x{cmd_id:02X}")

        self._phy = phy
        self._channel = channel

    def set_channel(self, channel: int) -> None:
        """Set RF channel"""
        self._send_command(Command.SET_CHANNEL, CommandBuilder.set_channel(channel))
        cmd_id, seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})

        if cmd_id == Response.ERROR:
            raise CommandError("Set channel failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to SET_CHANNEL: 0x{cmd_id:02X}")

        self._channel = channel

    def set_power(self, power_dbm: int) -> None:
        """Set TX power in dBm"""
        self._send_command(Command.SET_POWER, CommandBuilder.set_power(power_dbm))
        cmd_id, seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})

        if cmd_id == Response.ERROR:
            raise CommandError("Set power failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to SET_POWER: 0x{cmd_id:02X}")

    def set_ble_addr(self, addr: bytes) -> None:
        """Set BLE advertising TX address.

        Args:
            addr: 6-byte address in little-endian (e.g. b'\\x01\\xEE\\xDD\\xCC\\xBB\\xAA')
                  or use set_ble_addr_str("AA:BB:CC:DD:EE:01") for human-readable format.
        """
        self._send_command(Command.SET_BLE_ADDR, CommandBuilder.set_ble_addr(addr))
        cmd_id, seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})
        if cmd_id == Response.ERROR:
            raise CommandError("Set BLE addr failed", payload[0] if payload else 0)

    def set_ble_addr_str(self, addr_str: str) -> None:
        """Set BLE advertising TX address from string format.

        Args:
            addr_str: Address like "AA:BB:CC:DD:EE:FF" (as shown in nRF Connect).
                      Internally reversed to little-endian for the firmware.
        """
        parts = addr_str.split(":")
        if len(parts) != 6:
            raise ValueError("Address must be XX:XX:XX:XX:XX:XX format")
        addr_bytes = bytes(int(p, 16) for p in reversed(parts))
        self.set_ble_addr(addr_bytes)

    def set_ble_scan_mode(self, active: bool = True) -> None:
        """Set BLE scan mode: passive or active.

        Active scan sends SCAN_REQ to devices, which respond with SCAN_RSP
        containing additional data (full name, extra UUIDs, etc.).
        Must be called BEFORE start_rx().

        Args:
            active: True for active scan (SCAN_REQ/RSP), False for passive.
        """
        payload = bytes([1 if active else 0])
        self._send_command(Command.SET_BLE_SCAN_MODE, payload)
        cmd_id, seq, resp = self._read_response(expected={Response.ACK, Response.ERROR})
        if cmd_id == Response.ERROR:
            raise CommandError("Set BLE scan mode failed", resp[0] if resp else 0)

    def configure_prop(
        self,
        frequency_hz: int,
        mod_type: int = 1,
        symbol_rate: int = 50000,
        deviation: int = 100,
        rx_bw: int = 0x52,
        sync_word: int = 0x930B51DE,
        format_conf: int = 0,
    ) -> None:
        """Configure proprietary radio parameters.

        Args:
            frequency_hz: Frequency in Hz (e.g. 433920000 for 433.92 MHz)
            mod_type: Modulation type (0=FSK, 1=GFSK, 2=OOK/ASK, 4=MSK, 5=4-FSK, 6=4-GFSK)
            symbol_rate: Symbol rate in baud (e.g. 4800, 50000)
            deviation: Deviation register value (for FSK/GFSK)
            rx_bw: RX bandwidth register value
            sync_word: 32-bit sync word
            format_conf: Raw formatConf bitfield (0=use defaults)

        Warning:
            OOK mode (mod_type=2) loads dedicated RF core patches that lock
            the radio to OOK. After configuring OOK, the device is locked to
            that frequency and modulation. Power cycle required to use any
            other mode or change OOK frequency. Set your target frequency
            in the first configure_prop(mod_type=2) call.
        """
        if mod_type == 2:
            import warnings

            warnings.warn(
                "OOK mode locks the radio — power cycle required to change "
                "frequency or switch to other modes. Set target frequency now.",
                stacklevel=2,
            )
        self._send_command(
            Command.SET_PROP_CONFIG,
            CommandBuilder.set_prop_config(
                frequency_hz, mod_type, symbol_rate, deviation, rx_bw, sync_word, format_conf
            ),
        )
        cmd_id, seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})

        if cmd_id == Response.ERROR:
            raise CommandError("Set prop config failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to SET_PROP_CONFIG: 0x{cmd_id:02X}")

    def set_adv_hop(self, enabled: bool) -> None:
        """Enable or disable BLE advertising channel hopping during RX.

        This is part of the stable BLE RX API, but only applies to BLE
        advertising-channel reception.
        """
        self._send_command(Command.SET_ADV_HOP, CommandBuilder.set_adv_hop(enabled))
        cmd_id, seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})

        if cmd_id == Response.ERROR:
            raise CommandError("Set ADV hop failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to SET_ADV_HOP: 0x{cmd_id:02X}")

    def get_stats(self, timeout: float = 2.0) -> DeviceStats:
        """Get device RX metrics"""
        self._send_command(Command.GET_STATS)
        cmd_id, seq, payload = self._read_response(
            timeout=timeout, expected={Response.STATS, Response.ERROR}
        )

        if cmd_id == Response.ERROR:
            raise CommandError("Get stats failed", payload[0] if payload else 0)
        if cmd_id != Response.STATS:
            raise ProtocolError(f"Unexpected response to GET_STATS: 0x{cmd_id:02X}")
        if len(payload) < 16:
            raise ProtocolError(f"STATS payload too short: {len(payload)}")

        return DeviceStats(
            rx_ok=int.from_bytes(payload[0:4], "little"),
            rx_crc_err=int.from_bytes(payload[4:8], "little"),
            rx_drop=int.from_bytes(payload[8:12], "little"),
            rx_overflow=int.from_bytes(payload[12:16], "little"),
            ll_kind_unknown=(
                int.from_bytes(payload[16:20], "little")
                if len(payload) >= 36 and (self._capabilities & self.CAPABILITY_LL_STATS_EXT)
                else None
            ),
            ll_kind_adv=(
                int.from_bytes(payload[20:24], "little")
                if len(payload) >= 36 and (self._capabilities & self.CAPABILITY_LL_STATS_EXT)
                else None
            ),
            ll_kind_scan=(
                int.from_bytes(payload[24:28], "little")
                if len(payload) >= 36 and (self._capabilities & self.CAPABILITY_LL_STATS_EXT)
                else None
            ),
            ll_kind_connect=(
                int.from_bytes(payload[28:32], "little")
                if len(payload) >= 36 and (self._capabilities & self.CAPABILITY_LL_STATS_EXT)
                else None
            ),
            ll_kind_data=(
                int.from_bytes(payload[32:36], "little")
                if len(payload) >= 36 and (self._capabilities & self.CAPABILITY_LL_STATS_EXT)
                else None
            ),
        )

    def start_rx(self) -> None:
        """Start receiving"""
        self._send_command(Command.RX_START)
        cmd_id, seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})

        if cmd_id == Response.ERROR:
            raise CommandError("Start RX failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to RX_START: 0x{cmd_id:02X}")

    def stop_rx(self, retries: int = 3, timeout: float = 1.0) -> None:
        """Stop receiving"""
        last_timeout: Optional[TimeoutError] = None

        if retries <= 0:
            retries = 1

        for _ in range(retries):
            self._send_command(Command.RX_STOP)
            try:
                cmd_id, seq, payload = self._read_response(
                    timeout=timeout, expected={Response.ACK, Response.ERROR}
                )
            except TimeoutError as exc:
                last_timeout = exc
                continue

            if cmd_id == Response.ERROR:
                raise CommandError("Stop RX failed", payload[0] if payload else 0)
            if cmd_id != Response.ACK:
                raise ProtocolError(f"Unexpected response to RX_STOP: 0x{cmd_id:02X}")
            return

        if last_timeout is not None:
            raise last_timeout
        raise TimeoutError("Response timeout")

    def read_packets(self, timeout: Optional[float] = 1.0) -> Iterator[Packet]:
        """
        Read received packets.

        If timeout is a float, it is treated as a total read window in seconds.
        If timeout is None, packets are streamed indefinitely until caller stops iteration.
        """
        end_time = None if timeout is None else (time.monotonic() + max(timeout, 0.0))

        while True:
            if end_time is not None:
                remaining = end_time - time.monotonic()
                if remaining <= 0.0:
                    break
                read_timeout = remaining
            else:
                read_timeout = None

            try:
                cmd_id, seq, payload = self._read_response(read_timeout)

                if cmd_id == Response.RX_PACKET:
                    # Parse packet
                    if len(payload) >= 14:
                        timestamp = int.from_bytes(payload[0:8], "little")
                        channel = payload[8]
                        rssi = payload[9] - 256 if payload[9] > 127 else payload[9]
                        lqi = payload[10]
                        crc_ok = payload[11] == 1
                        pkt_len = payload[12]
                        data = payload[13 : 13 + pkt_len]
                        ll_pdu_kind = None
                        ll_pdu_type = None
                        ll_pdu_flags = None
                        ll_meta_offset = 13 + pkt_len

                        if (
                            self._capabilities & self.CAPABILITY_LL_PDU_META
                            and len(payload) >= ll_meta_offset + 2
                        ):
                            ll_pdu_kind = payload[ll_meta_offset]
                            ll_pdu_type = payload[ll_meta_offset + 1]
                            if len(payload) >= ll_meta_offset + 3:
                                ll_pdu_flags = payload[ll_meta_offset + 2]

                        yield Packet(
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
            except TimeoutError:
                break

    def transmit(self, packet: bytes, power_dbm: int = -128, timeout: float = 5.0) -> None:
        """Transmit a packet"""
        self._send_command(Command.TX_RAW, CommandBuilder.tx_raw(packet, power_dbm))
        cmd_id, seq, payload = self._read_response(
            timeout=timeout, expected={Response.ACK, Response.ERROR}
        )

        if cmd_id == Response.ERROR:
            raise CommandError("Transmit failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to TX_RAW: 0x{cmd_id:02X}")

    def transmit_frame(self, packet: bytes, timeout: float = 5.0) -> None:
        """Transmit payload using firmware framing for current PHY."""
        if len(packet) == 0:
            raise ValueError("packet must not be empty")

        self._send_command(Command.TX_FRAME, CommandBuilder.tx_frame(packet))
        cmd_id, seq, payload = self._read_response(
            timeout=timeout, expected={Response.ACK, Response.ERROR}
        )

        if cmd_id == Response.ERROR:
            raise CommandError("Transmit frame failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to TX_FRAME: 0x{cmd_id:02X}")

    def transmit_burst(
        self,
        packet: bytes,
        count: int,
        interval_us: int = 0,
        timeout: float = 5.0,
    ) -> None:
        """Schedule a TX burst in firmware."""
        if len(packet) == 0:
            raise ValueError("packet must not be empty")
        if count <= 0 or count > 0xFFFF:
            raise ValueError("count must be in range 1..65535")
        if interval_us < 0 or interval_us > 0xFFFFFFFF:
            raise ValueError("interval_us must be in range 0..4294967295")

        self._send_command(Command.TX_BURST, CommandBuilder.tx_burst(packet, count, interval_us))
        cmd_id, seq, payload = self._read_response(
            timeout=timeout, expected={Response.ACK, Response.ERROR}
        )

        if cmd_id == Response.ERROR:
            raise CommandError("Transmit burst failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to TX_BURST: 0x{cmd_id:02X}")

    def transmit_continuous(
        self,
        packet: bytes,
        interval_us: int = 0,
        timeout: float = 5.0,
    ) -> None:
        """Start continuous TX in firmware until TX_STOP."""
        if len(packet) == 0:
            raise ValueError("packet must not be empty")
        if interval_us < 0 or interval_us > 0xFFFFFFFF:
            raise ValueError("interval_us must be in range 0..4294967295")

        self._send_command(Command.TX_CONTINUOUS, CommandBuilder.tx_continuous(packet, interval_us))
        cmd_id, seq, payload = self._read_response(
            timeout=timeout, expected={Response.ACK, Response.ERROR}
        )

        if cmd_id == Response.ERROR:
            raise CommandError("Transmit continuous failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to TX_CONTINUOUS: 0x{cmd_id:02X}")

    def stop_transmit(self, timeout: float = 5.0) -> None:
        """Stop active TX stream (continuous/burst when supported by firmware)."""
        self._send_command(Command.TX_STOP, CommandBuilder.tx_stop())
        cmd_id, seq, payload = self._read_response(
            timeout=timeout, expected={Response.ACK, Response.ERROR}
        )

        if cmd_id == Response.ERROR:
            raise CommandError("TX_STOP failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to TX_STOP: 0x{cmd_id:02X}")

    def start_jam(
        self,
        channel: int,
        power_dbm: int = 20,
        duration_ms: int = 3000,
        timeout: float = 5.0,
    ) -> None:
        """Experimental: start a continuous jamming-style TX stream.

        This command exists in firmware, but it is not part of the current
        stable RF baseline because "ACK" is not the same as validated
        interference against real signals.
        """
        if channel < 0 or channel > 255:
            raise ValueError("channel must be in range 0..255")
        if duration_ms <= 0 or duration_ms > 30000:
            raise ValueError("duration_ms must be in range 1..30000")

        self._send_command(
            Command.JAM_CONTINUOUS,
            CommandBuilder.jam_continuous(channel, power_dbm, duration_ms),
        )
        cmd_id, seq, payload = self._read_response(
            timeout=timeout, expected={Response.ACK, Response.ERROR}
        )

        if cmd_id == Response.ERROR:
            raise CommandError("JAM_CONTINUOUS failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to JAM_CONTINUOUS: 0x{cmd_id:02X}")

    def stop_jam(self, timeout: float = 5.0) -> None:
        """Experimental: stop an active jamming stream."""
        last_exc: Optional[Exception] = None

        for _ in range(2):
            try:
                self._send_command(Command.JAM_STOP, CommandBuilder.jam_stop())
                cmd_id, seq, payload = self._read_response(
                    timeout=timeout, expected={Response.ACK, Response.ERROR}
                )

                if cmd_id == Response.ERROR:
                    raise CommandError("JAM_STOP failed", payload[0] if payload else 0)
                if cmd_id != Response.ACK:
                    raise ProtocolError(f"Unexpected response to JAM_STOP: 0x{cmd_id:02X}")
                return
            except (TimeoutError, CommandError, ProtocolError) as exc:
                last_exc = exc
                time.sleep(0.05)

        # Fallback path: firmware route may still honor TX_STOP even if JAM_STOP path is busy.
        try:
            self.stop_transmit(timeout=timeout)
            return
        except (TimeoutError, CommandError, ProtocolError):
            if last_exc is not None:
                raise last_exc
            raise

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()
        return False
