"""
FeralRF - Radio interface
"""

from dataclasses import dataclass
from typing import Iterator, Optional

import serial
import serial.tools.list_ports

from feralrf.commands import CommandBuilder
from feralrf.enums import PHY, Command, Response
from feralrf.exceptions import CommandError, ConnectionError, TimeoutError
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


@dataclass
class DeviceInfo:
    """Device information"""

    firmware_version: str
    capabilities: int
    serial: str


class Radio:
    """
    Synchronous Radio interface for FeralRF
    """

    def __init__(self, port: Optional[str] = None, baudrate: int = 921600):
        self.port = port
        self.baudrate = baudrate
        self._serial: Optional[serial.Serial] = None
        self._seq = 0
        self._phy: Optional[PHY] = None
        self._channel: int = 0
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

    def _read_response(self, timeout: float = 1.0) -> tuple:
        """Read and parse a response"""
        if not self._serial:
            raise ConnectionError("Not connected")

        self._serial.timeout = timeout

        # Read until we get a complete frame (0x00 delimiter)
        while True:
            byte = self._serial.read(1)
            if not byte:
                raise TimeoutError("Response timeout")

            self._rx_buffer.extend(byte)

            if byte == b"\x00" and len(self._rx_buffer) > 1:
                # Try to decode frame
                try:
                    decoded = cobs_decode(bytes(self._rx_buffer))
                    self._rx_buffer.clear()
                    return parse_frame(decoded)
                except ValueError:
                    # Invalid frame, continue reading
                    continue

    def init(self) -> DeviceInfo:
        """Initialize radio and get device info"""
        self.connect()
        self._send_command(Command.RADIO_INIT)

        # Wait for ACK
        cmd_id, seq, payload = self._read_response()
        if cmd_id == Response.ERROR:
            raise CommandError("Radio init failed", payload[0] if payload else 0)

        # Get device info
        self._send_command(Command.GET_INFO)
        cmd_id, seq, payload = self._read_response()

        if cmd_id == Response.INFO:
            # Parse info response
            return DeviceInfo(
                firmware_version=f"{payload[0]}.{payload[1]}.{payload[2]}",
                capabilities=payload[3] if len(payload) > 3 else 0,
                serial=payload[4:12].hex() if len(payload) > 4 else "",
            )

        return DeviceInfo(firmware_version="unknown", capabilities=0, serial="")

    def set_phy(self, phy: PHY, channel: int = 0) -> None:
        """Set PHY type and channel"""
        self._send_command(Command.SET_PHY, CommandBuilder.set_phy(phy, channel))
        cmd_id, seq, payload = self._read_response()

        if cmd_id == Response.ERROR:
            raise CommandError("Set PHY failed", payload[0] if payload else 0)

        self._phy = phy
        self._channel = channel

    def set_channel(self, channel: int) -> None:
        """Set RF channel"""
        self._send_command(Command.SET_CHANNEL, CommandBuilder.set_channel(channel))
        cmd_id, seq, payload = self._read_response()

        if cmd_id == Response.ERROR:
            raise CommandError("Set channel failed", payload[0] if payload else 0)

        self._channel = channel

    def set_power(self, power_dbm: int) -> None:
        """Set TX power in dBm"""
        self._send_command(Command.SET_POWER, CommandBuilder.set_power(power_dbm))
        cmd_id, seq, payload = self._read_response()

        if cmd_id == Response.ERROR:
            raise CommandError("Set power failed", payload[0] if payload else 0)

    def start_rx(self) -> None:
        """Start receiving"""
        self._send_command(Command.RX_START)
        cmd_id, seq, payload = self._read_response()

        if cmd_id == Response.ERROR:
            raise CommandError("Start RX failed", payload[0] if payload else 0)

    def stop_rx(self) -> None:
        """Stop receiving"""
        self._send_command(Command.RX_STOP)
        cmd_id, seq, payload = self._read_response()

        if cmd_id == Response.ERROR:
            raise CommandError("Stop RX failed", payload[0] if payload else 0)

    def read_packets(self, timeout: float = 1.0) -> Iterator[Packet]:
        """Read received packets"""
        while True:
            try:
                cmd_id, seq, payload = self._read_response(timeout)

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

                        yield Packet(
                            timestamp_us=timestamp,
                            channel=channel,
                            rssi_dbm=rssi,
                            lqi=lqi,
                            crc_ok=crc_ok,
                            data=data,
                        )
            except TimeoutError:
                break

    def transmit(self, packet: bytes, power_dbm: int = -128) -> None:
        """Transmit a packet"""
        self._send_command(Command.TX_RAW, CommandBuilder.tx_raw(packet, power_dbm))
        cmd_id, seq, payload = self._read_response()

        if cmd_id == Response.ERROR:
            raise CommandError("Transmit failed", payload[0] if payload else 0)

    def __enter__(self):
        self.connect()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()
        return False
