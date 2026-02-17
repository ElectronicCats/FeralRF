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
    Synchronous Radio interface for FeralRF
    """

    CAPABILITY_RX_STATS = 0x01
    CAPABILITY_LL_PDU_META = 0x02
    CAPABILITY_LL_STATS_EXT = 0x04

    def __init__(self, port: Optional[str] = None, baudrate: int = 921600):
        self.port = port
        self.baudrate = baudrate
        self._serial: Optional[serial.Serial] = None
        self._seq = 0
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
        except serial.SerialException as e:
            raise ConnectionError(f"Failed to connect to {self.port}: {e}")

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

        frame = build_frame(cmd, self._next_seq(), payload)
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
        self.connect()
        self._send_command(Command.RADIO_INIT)

        # Wait for ACK/ERROR
        cmd_id, seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})
        if cmd_id == Response.ERROR:
            raise CommandError("Radio init failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to RADIO_INIT: 0x{cmd_id:02X}")

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

    def set_phy(self, phy: PHY, channel: int = 0) -> None:
        """Set PHY type and channel"""
        self._send_command(Command.SET_PHY, CommandBuilder.set_phy(phy, channel))
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

    def transmit(self, packet: bytes, power_dbm: int = -128) -> None:
        """Transmit a packet"""
        self._send_command(Command.TX_RAW, CommandBuilder.tx_raw(packet, power_dbm))
        cmd_id, seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})

        if cmd_id == Response.ERROR:
            raise CommandError("Transmit failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to TX_RAW: 0x{cmd_id:02X}")

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()
        return False
