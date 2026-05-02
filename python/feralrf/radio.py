"""
FeralRF - Radio interface
"""

import time
from dataclasses import dataclass
from typing import Iterator, Optional, Set

import serial
import serial.tools.list_ports

from feralrf._ble_scan import BleScanResult, extract_pdu_header
from feralrf._responses import DebugConnParamsResponse, DebugTimingResponse
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


@dataclass
class ConnectionResult:
    """Result of a BLE CMD_CONNECT attempt.

    Result codes mirror the firmware:
        0: OK
        1: TIMEOUT
        2: NO_SYNC
        3: RF_ERR
    """

    result: int

    @property
    def is_ok(self) -> bool:
        return self.result == 0


@dataclass
class ConnectionStatus:
    """Snapshot of the current BLE central connection.

    Fields after `last_status` are only populated when the firmware
    includes the extended debug block (F7 telemetry; may be removed
    after F8 validation).
    """

    connected: bool
    interval: int
    events: int
    last_status: int
    tx_done: Optional[int] = None
    att_state: Optional[int] = None
    total_rx: Optional[int] = None
    conn_time: Optional[int] = None  # RAT-tick origin of the connection anchor


@dataclass
class GattService:
    """A GATT primary service discovered on the peer.

    uuid is the raw LE bytes as reported by the peer: 2 bytes for a
    16-bit UUID, 16 bytes for a full UUID.
    """

    start_handle: int
    end_handle: int
    uuid: bytes


@dataclass
class GattCharacteristic:
    """A GATT characteristic discovered on the peer."""

    handle: int
    properties: int
    value_handle: int
    uuid: bytes


@dataclass
class GattNotification:
    """An ATT notification or indication received on a subscribed CCC.

    Async push from firmware: when the peer sends ATT op 0x1B (NOTIFY)
    or 0x1D (INDICATE), the firmware emits RSP_GATT_NOTIFY[handle:2][value:N]
    immediately — no host poll required. The host's read_gatt_notifications()
    iterator yields these as they arrive.
    """

    handle: int
    value: bytes
    timestamp: float  # host monotonic at receive


@dataclass
class LLPacket:
    """A captured LL data PDU from a followed BLE connection.

    Emitted by the firmware's connection follower (CMD_FOLLOW_START) for
    every data-channel PDU it captures from a non-FeralRF central↔peripheral
    link. Capture-only — the firmware never injects on followed links.

    direction is "M->S" or "S->M" inferred from the LL header SN/NESN
    transitions, or "?" if the firmware could not determine direction.
    payload is the raw LL PDU starting at the 2-byte LL header.
    """

    direction: str
    channel: int
    rssi_dbm: int
    event_counter: int
    payload: bytes
    timestamp: float  # host monotonic at receive


@dataclass
class GattDiscoveryResult:
    """Aggregated output of a full gatt_discover() call."""

    services: list
    characteristics: list
    status: int


class Radio:
    """
    Synchronous Radio interface for FeralRF.

    Public API status:
        Stable:
            init, set_phy, set_channel, set_power, start_rx, read_packets,
            stop_rx, transmit, transmit_frame, transmit_burst,
            transmit_continuous, stop_transmit, get_stats, configure_prop,
            scan_ble_active, set_ble_addr, set_ble_addr_str,
            set_ble_scan_mode, set_adv_hop, reset_device, ble_connect,
            ble_disconnect, conn_status, gatt_discover, gatt_read,
            gatt_write.
        Experimental:
            start_jam, stop_jam.
        Pending:
            spectrum helpers, non-BLE attack helpers in host.
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
        "scan_ble_active",
        "set_ble_scan_mode",
        "set_adv_hop",
        "reset_device",
        "ble_connect",
        "ble_disconnect",
        "conn_status",
        "gatt_discover",
        "gatt_read",
        "gatt_write",
        "gatt_subscribe",
        "read_gatt_notifications",
    )
    EXPERIMENTAL_METHODS = (
        "start_jam",
        "stop_jam",
    )
    PENDING_FEATURES = ("spectrum",)

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
            # 0x1209 = CatSniffer v3 (Electronic Cats VID, Cat-Bridge)
            if port.vid in (0x2E8A, 0x2341, 0x1A86, 0x10C4, 0x1209):
                # For CatSniffer v3, only the Cat-Bridge CDC is the radio link.
                if port.vid == 0x1209:
                    desc = (port.description or "") + " " + (port.product or "")
                    if "Bridge" not in desc:
                        continue
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
        """Get next sequence number. 0xFF is reserved for firmware async errors."""
        seq = self._seq
        nxt = (seq + 1) & 0xFF
        if nxt == 0xFF:
            nxt = 0
        self._seq = nxt
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

    def ble_connect(
        self, addr_le: bytes, addr_type: int, timeout: float = 8.0
    ) -> "ConnectionResult":
        """Issue CMD_CONNECT as BLE central; blocks until RSP_CONN_RESULT.

        Args:
            addr_le: 6-byte peer address in little-endian wire order
                (reversed of AA:BB:CC:DD:EE:FF).
            addr_type: 0 for public, 1 for random.
            timeout: Seconds to wait for RSP_CONN_RESULT (firmware initiator
                may block up to 5 s per connect attempt).
        """
        self._send_command(
            Command.CONNECT,
            CommandBuilder.ble_connect(addr_le, addr_type),
        )
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.CONN_RESULT, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("CONNECT failed", payload[0] if payload else 0)
        if cmd_id != Response.CONN_RESULT:
            raise ProtocolError(f"Unexpected response to CONNECT: 0x{cmd_id:02X}")
        if not payload:
            raise ProtocolError("CONN_RESULT payload empty")
        return ConnectionResult(result=payload[0])

    def ble_disconnect(self, timeout: float = 2.0) -> None:
        """Issue CMD_DISCONNECT; firmware returns to idle."""
        self._send_command(Command.DISCONNECT, CommandBuilder.ble_disconnect())
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("DISCONNECT failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to DISCONNECT: 0x{cmd_id:02X}")

    def conn_status(self, timeout: float = 2.0) -> "ConnectionStatus":
        """Issue CMD_CONN_STATUS and return the parsed ConnectionStatus.

        The firmware always returns at least 9 bytes. The extra F7 debug
        fields (tx_done, att_state, total_rx) are optional and populated
        only when the firmware includes them.
        """
        self._send_command(Command.CONN_STATUS, CommandBuilder.conn_status())
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.CONN_STATUS, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("CONN_STATUS failed", payload[0] if payload else 0)
        if cmd_id != Response.CONN_STATUS:
            raise ProtocolError(f"Unexpected response to CONN_STATUS: 0x{cmd_id:02X}")
        if len(payload) < 9:
            raise ProtocolError(f"CONN_STATUS payload too short: {len(payload)}")

        connected = bool(payload[0])
        interval = int.from_bytes(payload[1:3], "little")
        events = int.from_bytes(payload[5:7], "little")
        last_status = int.from_bytes(payload[7:9], "little")

        tx_done = att_state = total_rx = conn_time = None
        if len(payload) >= 14:
            tx_done = int.from_bytes(payload[9:11], "little")
            att_state = payload[11]
            total_rx = int.from_bytes(payload[12:14], "little")
        if len(payload) >= 18:
            conn_time = int.from_bytes(payload[14:18], "little")

        return ConnectionStatus(
            connected=connected,
            interval=interval,
            events=events,
            last_status=last_status,
            tx_done=tx_done,
            att_state=att_state,
            total_rx=total_rx,
            conn_time=conn_time,
        )

    def debug_timing(self, timeout: float = 2.0) -> DebugTimingResponse:
        """Issue CMD_DEBUG_TIMING; firmware returns the last N master-event timing records."""
        self._send_command(Command.DEBUG_TIMING, CommandBuilder.debug_timing())
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.DEBUG_TIMING, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("DEBUG_TIMING failed", payload[0] if payload else 0)
        if cmd_id != Response.DEBUG_TIMING:
            raise ProtocolError(f"Unexpected response to DEBUG_TIMING: 0x{cmd_id:02X}")
        return DebugTimingResponse.parse(payload)

    def debug_conn_params(self, timeout: float = 2.0) -> DebugConnParamsResponse:
        """Issue CMD_DEBUG_CONN_PARAMS; dumps post-initiator s_state + s_ll_data.

        Used for Session 5 wire-vs-state diagnostics. Compare the returned
        ``ll_data`` (or ``ll_data_decoded()``) against a Sniffle pcap of the
        same CONNECT_IND on the wire to spot SDK-rewrite mismatches.
        """
        self._send_command(Command.DEBUG_CONN_PARAMS, CommandBuilder.debug_conn_params())
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.DEBUG_CONN_PARAMS, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("DEBUG_CONN_PARAMS failed", payload[0] if payload else 0)
        if cmd_id != Response.DEBUG_CONN_PARAMS:
            raise ProtocolError(f"Unexpected response to DEBUG_CONN_PARAMS: 0x{cmd_id:02X}")
        return DebugConnParamsResponse.parse(payload)

    def gatt_discover(self, timeout: float = 15.0) -> "GattDiscoveryResult":
        """Issue CMD_GATT_DISCOVER and collect the streamed services + chars.

        Firmware responds with:
            1. RSP_ACK (acknowledge discovery started)
            2. Interleaved RSP_GATT_SERVICE / RSP_GATT_CHAR
            3. RSP_GATT_DONE with status byte
        All stream frames carry the original request's seq
        (firmware sets s_gatt_seq = seq).
        """
        self._send_command(Command.GATT_DISCOVER, CommandBuilder.gatt_discover())

        cmd_id, _seq, payload = self._read_response(
            timeout=min(timeout, 3.0),
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_DISCOVER failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to GATT_DISCOVER: 0x{cmd_id:02X}")

        services: list = []
        characteristics: list = []
        status = 0xFF
        deadline = time.monotonic() + timeout

        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            cmd_id, _seq, payload = self._read_response(
                timeout=remaining,
                expected={
                    Response.GATT_SERVICE,
                    Response.GATT_CHAR,
                    Response.GATT_DONE,
                    Response.CONN_STATUS,
                    Response.ERROR,
                },
            )

            if cmd_id == Response.GATT_SERVICE:
                if len(payload) < 6:
                    raise ProtocolError(f"GATT_SERVICE payload too short: {len(payload)}")
                start_h = int.from_bytes(payload[0:2], "little")
                end_h = int.from_bytes(payload[2:4], "little")
                services.append(
                    GattService(start_handle=start_h, end_handle=end_h, uuid=bytes(payload[4:]))
                )
            elif cmd_id == Response.GATT_CHAR:
                if len(payload) < 7:
                    raise ProtocolError(f"GATT_CHAR payload too short: {len(payload)}")
                handle = int.from_bytes(payload[0:2], "little")
                props = payload[2]
                val_handle = int.from_bytes(payload[3:5], "little")
                characteristics.append(
                    GattCharacteristic(
                        handle=handle,
                        properties=props,
                        value_handle=val_handle,
                        uuid=bytes(payload[5:]),
                    )
                )
            elif cmd_id == Response.GATT_DONE:
                status = payload[0] if payload else 0xFF
                break
            elif cmd_id == Response.CONN_STATUS:
                continue
            elif cmd_id == Response.ERROR:
                raise CommandError("GATT_DISCOVER stream error", payload[0] if payload else 0)

        return GattDiscoveryResult(
            services=services,
            characteristics=characteristics,
            status=status,
        )

    def gatt_read(self, handle: int, timeout: float = 5.0) -> bytes:
        """Issue CMD_GATT_READ for the given attribute handle; return the value bytes."""
        self._send_command(Command.GATT_READ, CommandBuilder.gatt_read(handle))

        cmd_id, _seq, payload = self._read_response(
            timeout=min(timeout, 3.0),
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_READ failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to GATT_READ: 0x{cmd_id:02X}")

        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.GATT_READ_VALUE, Response.GATT_DONE, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_READ value error", payload[0] if payload else 0)
        if cmd_id == Response.GATT_DONE:
            status = payload[0] if payload else 0xFF
            raise CommandError("GATT_READ done without value", status)
        if len(payload) < 2:
            raise ProtocolError(f"GATT_READ_VALUE payload too short: {len(payload)}")
        return bytes(payload[2:])

    def gatt_write(self, handle: int, data: bytes, timeout: float = 5.0) -> int:
        """Issue CMD_GATT_WRITE; return the firmware status byte (0 = OK)."""
        self._send_command(Command.GATT_WRITE, CommandBuilder.gatt_write(handle, data))

        cmd_id, _seq, payload = self._read_response(
            timeout=min(timeout, 3.0),
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_WRITE failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to GATT_WRITE: 0x{cmd_id:02X}")

        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.GATT_DONE, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_WRITE ack error", payload[0] if payload else 0)
        return payload[0] if payload else 0xFF

    def gatt_subscribe(
        self,
        handle: int,
        enable: bool = True,
        indicate: bool = False,
        timeout: float = 3.0,
    ) -> None:
        """Subscribe to notifications (or indications) for a GATT characteristic.

        Sends CMD_GATT_SUBSCRIBE which the firmware handles by writing
        0x0001 (notify) or 0x0002 (indicate) — or 0x0000 if disabling — to
        the CCC descriptor at (handle + 1).

        Args:
            handle: The characteristic VALUE handle (not declaration).
                Firmware writes the CCC at handle + 1 per BLE convention.
            enable: True to subscribe, False to unsubscribe.
            indicate: True for indications (0x0002), False for notifications (0x0001).
                Ignored when enable=False.
            timeout: Seconds to wait for ACK.
        """
        self._send_command(
            Command.GATT_SUBSCRIBE,
            CommandBuilder.gatt_subscribe(handle, enable, indicate),
        )
        cmd_id, _seq, payload = self._read_response(
            timeout=timeout,
            expected={Response.ACK, Response.ERROR},
        )
        if cmd_id == Response.ERROR:
            raise CommandError("GATT_SUBSCRIBE failed", payload[0] if payload else 0)
        if cmd_id != Response.ACK:
            raise ProtocolError(f"Unexpected response to GATT_SUBSCRIBE: 0x{cmd_id:02X}")

    def read_gatt_notifications(
        self,
        timeout: float = 5.0,
    ) -> "Iterator[GattNotification]":
        """Yield GattNotification frames as they arrive.

        Iterates over RX frames from the firmware, filtering for
        RSP_GATT_NOTIFY. Other unsolicited frames (e.g., RSP_DISCONNECTED)
        end the iterator. Iterator ends quietly on timeout — caller can
        loop and call again.

        Args:
            timeout: Seconds to wait for the next frame. The iterator
                ends when this elapses without a new RSP_GATT_NOTIFY.

        Yields:
            GattNotification per received frame.

        Note:
            If the host is slower than the peripheral, the COBS RX buffer
            can overflow and frames are lost. Backpressure is deferred to
            F8c.
        """
        while True:
            try:
                cmd_id, _seq, payload = self._read_response(
                    timeout=timeout,
                    expected={Response.GATT_NOTIFY},
                )
            except TimeoutError:
                return
            if cmd_id != Response.GATT_NOTIFY:
                return
            if len(payload) < 2:
                # Malformed — skip
                continue
            handle = int.from_bytes(payload[0:2], "little")
            value = bytes(payload[2:])
            yield GattNotification(
                handle=handle,
                value=value,
                timestamp=time.monotonic(),
            )

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

    def scan_ble_active(
        self,
        duration: float,
        channels=(37, 38, 39),
        phy: PHY = PHY.BLE_1M,
    ) -> dict[str, BleScanResult]:
        """Active BLE scan — send SCAN_REQ, capture SCAN_RSP, merge per MAC.

        On exit (try/finally), restores prior PHY/channel and resets
        set_ble_scan_mode and set_adv_hop to False (no shadow state for
        those — the firmware defaults to passive scan + no hop), even on exception.

        Args:
            duration: seconds to listen.
            channels: int or sequence of advertising channels (37/38/39).
                      Single channel → adv_hop disabled.
                      Multiple channels → adv_hop enabled, scan starts at channels[0].
            phy: BLE PHY (default BLE_1M).

        Returns:
            dict[str, BleScanResult] keyed by MAC display string.
        """
        if isinstance(channels, int):
            channels = (channels,)
        else:
            channels = tuple(channels)
        if not channels:
            raise ValueError("channels must contain at least one channel")

        prior_phy = self._phy
        prior_channel = self._channel
        hop_needed = len(channels) > 1
        results: dict[str, BleScanResult] = {}

        try:
            self.set_ble_scan_mode(active=True)
            self.set_adv_hop(hop_needed)
            self.set_phy(phy, channel=channels[0])
            self.start_rx()

            for pkt in self.read_packets(timeout=duration):
                if not pkt.crc_ok:
                    continue
                if len(pkt.data) < 8:
                    continue
                # Only BLE adv-channel PDUs have ll_pdu_type set; non-BLE
                # data has ll_pdu_type None. Filter to adv/scan kinds.
                if pkt.ll_pdu_type is None:
                    continue
                # PDU types: 0x00 ADV_IND, 0x01 ADV_DIRECT, 0x02 ADV_NONCONN_IND,
                # 0x04 SCAN_RSP, 0x06 ADV_SCAN_IND, 0x07 ADV_EXT_IND.
                # 0x03 SCAN_REQ and 0x05 CONNECT_IND are not what we expect from a peripheral.
                if pkt.ll_pdu_type not in (0x00, 0x01, 0x02, 0x04, 0x06, 0x07):
                    continue

                mac, addr_type = extract_pdu_header(pkt.data)
                if mac is None:
                    continue
                result = results.get(mac)
                if result is None:
                    result = BleScanResult(mac=mac, addr_type=addr_type)
                    results[mac] = result
                result.update_from_packet(pkt)
        finally:
            try:
                self.stop_rx()
            except Exception:
                pass
            try:
                self.set_ble_scan_mode(active=False)
            except Exception:
                pass
            try:
                self.set_adv_hop(False)
            except Exception:
                pass
            try:
                if prior_phy is not None:
                    self.set_phy(prior_phy, channel=prior_channel)
            except Exception:
                pass

        return results

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

    def tx_cw(self, power_dbm: int = 0) -> None:
        """Emit unmodulated carrier on current PHY/channel.

        Requires set_phy(...) first to select band + channel/frequency.
        Stop with tx_test_stop(). Test signal runs until cancelled.

        Args:
            power_dbm: TX power, -20 to +5 dBm (std-PA cap on this hw rev).

        Raises:
            CommandError: if no PHY is set or RF Core rejects the command.
        """
        self.set_power(power_dbm)
        self._send_command(Command.TX_CW)
        cmd_id, _seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})
        if cmd_id == Response.ERROR:
            raise CommandError("tx_cw failed", payload[0] if payload else 0)

    def tx_prbs(self, power_dbm: int = 0, pattern: str = "prbs15") -> None:
        """Emit PRBS-modulated test signal on current PHY/channel.

        Args:
            power_dbm: TX power, -20 to +5 dBm.
            pattern: 'prbs15' (default, longer-period spectral test) or
                'prbs32'. Note: BLE DTM PRBS-9 is a different command
                (CMD_BLE5_TX_TEST) and is NOT exposed here — this method
                wraps the generic CMD_TX_TEST which only offers PRBS-15/32.

        Raises:
            ValueError: if pattern not 'prbs15' or 'prbs32'.
            CommandError: if no PHY is set or RF Core rejects the command.
        """
        mode_byte = {"prbs15": 0x01, "prbs32": 0x02}.get(pattern.lower())
        if mode_byte is None:
            raise ValueError(f"pattern must be 'prbs15' or 'prbs32', got {pattern!r}")
        self.set_power(power_dbm)
        self._send_command(Command.TX_PRBS, bytes([mode_byte]))
        cmd_id, _seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})
        if cmd_id == Response.ERROR:
            raise CommandError("tx_prbs failed", payload[0] if payload else 0)

    def tx_test_stop(self) -> None:
        """Stop any active CW or PRBS test signal. Idempotent — safe to call
        when no test is running.

        Raises:
            CommandError: only if firmware reports an unexpected error.
        """
        self._send_command(Command.TX_TEST_STOP)
        cmd_id, _seq, payload = self._read_response(expected={Response.ACK, Response.ERROR})
        if cmd_id == Response.ERROR:
            raise CommandError("tx_test_stop failed", payload[0] if payload else 0)

    def random_bytes(self, n: int) -> bytes:
        """Generate `n` cryptographically secure random bytes from the chip's TRNG.

        Args:
            n: Number of bytes (1 <= n <= 240).

        Returns:
            `n` random bytes.

        Raises:
            ValueError: If `n` is outside [1, 240].
            CryptoError: If firmware TRNG is unavailable.
        """
        if not 1 <= n <= 240:
            raise ValueError(f"random_bytes: n must be in [1, 240], got {n}")
        self._send_command(Command.CMD_RANDOM, bytes([n]))
        rsp_id, _seq, data = self._read_response(expected={Response.RSP_RANDOM, Response.ERROR})
        if rsp_id == Response.ERROR:
            from feralrf.exceptions import CryptoError

            err = data[0] if data else 0
            raise CryptoError(f"random_bytes failed: err=0x{err:02X}")
        if len(data) != n:
            from feralrf.exceptions import CryptoError

            raise CryptoError(f"random_bytes returned {len(data)} bytes, expected {n}")
        return data

    def _aes_block_op(
        self,
        op: int,
        key: bytes,
        data: bytes,
        mode: str,
        iv: Optional[bytes] = None,
    ) -> bytes:
        from feralrf.exceptions import CryptoError

        if len(key) != 16:
            raise ValueError(f"key must be 16 bytes, got {len(key)}")
        if mode == "ecb":
            if len(data) != 16:
                raise ValueError(f"data must be 16 bytes for ECB, got {len(data)}")
            cmd = Command.CMD_AES_ECB
            payload = bytes([op]) + key + data
        elif mode == "ctr":
            if iv is None or len(iv) != 16:
                raise ValueError("iv must be 16 bytes for CTR")
            if len(data) > 200:  # firmware out[200] cap
                raise ValueError(f"data too large for one-shot CTR: {len(data)}")
            cmd = Command.CMD_AES_CTR
            payload = bytes([op]) + key + iv + data
        elif mode == "cbc":
            if iv is None or len(iv) != 16:
                raise ValueError("iv must be 16 bytes for CBC")
            if len(data) % 16 != 0 or len(data) == 0:
                raise ValueError(f"data must be non-empty multiple of 16 for CBC, got {len(data)}")
            if len(data) > 192:
                raise ValueError(f"data too large for one-shot CBC: {len(data)}")
            cmd = Command.CMD_AES_CBC
            payload = bytes([op]) + key + iv + data
        else:
            raise ValueError(f"unknown mode {mode!r}; expected ecb|ctr|cbc")

        self._send_command(cmd, payload)
        rsp_id, _seq, out = self._read_response(expected={Response.RSP_AES, Response.ERROR})
        if rsp_id == Response.ERROR:
            err = out[0] if out else 0
            raise CryptoError(f"aes {mode} failed: err=0x{err:02X}")
        return out

    def aes_encrypt(self, key: bytes, data: bytes, mode: str, iv: Optional[bytes] = None) -> bytes:
        """Encrypt `data` under `key` with mode 'ecb', 'ctr', or 'cbc'."""
        return self._aes_block_op(op=0, key=key, data=data, mode=mode, iv=iv)

    def aes_decrypt(self, key: bytes, data: bytes, mode: str, iv: Optional[bytes] = None) -> bytes:
        """Decrypt `data` under `key` with mode 'ecb', 'ctr', or 'cbc'."""
        return self._aes_block_op(op=1, key=key, data=data, mode=mode, iv=iv)

    def aes_ccm_encrypt(
        self, key: bytes, nonce: bytes, aad: bytes, plaintext: bytes, tag_len: int
    ) -> tuple:
        """Encrypt `plaintext` with AES-CCM. Returns (ciphertext, tag)."""
        return self._aes_ccm_op(
            op=0, key=key, nonce=nonce, aad=aad, data=plaintext, tag_in=b"", tag_len=tag_len
        )

    def aes_ccm_decrypt(
        self,
        key: bytes,
        nonce: bytes,
        aad: bytes,
        ciphertext: bytes,
        tag: bytes,
        tag_len: int,
    ) -> bytes:
        """Decrypt `ciphertext` with AES-CCM. Raises CryptoError on tag mismatch."""
        pt, _ = self._aes_ccm_op(
            op=1, key=key, nonce=nonce, aad=aad, data=ciphertext, tag_in=tag, tag_len=tag_len
        )
        return pt

    def _aes_ccm_op(self, op, key, nonce, aad, data, tag_in, tag_len):
        from feralrf.exceptions import CryptoError

        if len(key) != 16:
            raise ValueError(f"key must be 16 bytes, got {len(key)}")
        if not 7 <= len(nonce) <= 13:
            raise ValueError(f"nonce length must be 7..13, got {len(nonce)}")
        if tag_len not in (4, 6, 8, 10, 12, 14, 16):
            raise ValueError(f"tag_len must be in {{4,6,8,10,12,14,16}}, got {tag_len}")
        if len(aad) > 0xFFFF or len(data) > 0xFFFF:
            raise ValueError("aad/data length exceeds 16-bit limit")
        if op == 1 and len(tag_in) != tag_len:
            raise ValueError(f"tag length mismatch: expected {tag_len}, got {len(tag_in)}")

        header = bytes(
            [
                op,
                *key,
                len(nonce),
                *nonce,
                len(aad) & 0xFF,
                (len(aad) >> 8) & 0xFF,
                len(data) & 0xFF,
                (len(data) >> 8) & 0xFF,
                tag_len,
            ]
        )
        payload = header + aad + data + (tag_in if op == 1 else b"")

        self._send_command(Command.CMD_AES_CCM, payload)
        rsp_id, _seq, out = self._read_response(expected={Response.RSP_AES_CCM, Response.ERROR})
        if rsp_id == Response.ERROR:
            err = out[0] if out else 0
            if err == 0x02:
                raise CryptoError("aes_ccm: tag mismatch")
            raise CryptoError(f"aes_ccm failed: err=0x{err:02X}")
        if op == 0:
            ct = out[: len(data)]
            tag = out[len(data) : len(data) + tag_len]
            return (ct, tag)
        return (out, b"")

    def aes_gcm_encrypt(self, key: bytes, iv: bytes, aad: bytes, plaintext: bytes) -> tuple:
        """Encrypt with AES-GCM. Returns (ciphertext, 16-byte tag)."""
        return self._aes_gcm_op(op=0, key=key, iv=iv, aad=aad, data=plaintext, tag_in=b"")

    def aes_gcm_decrypt(
        self, key: bytes, iv: bytes, aad: bytes, ciphertext: bytes, tag: bytes
    ) -> bytes:
        """Decrypt with AES-GCM. Raises CryptoError on tag mismatch."""
        pt, _ = self._aes_gcm_op(op=1, key=key, iv=iv, aad=aad, data=ciphertext, tag_in=tag)
        return pt

    def sha256(self, data: bytes) -> bytes:
        """Compute SHA-256 of `data` (<=240 bytes). Returns 32-byte digest."""
        from feralrf.exceptions import CryptoError

        if len(data) > 240:
            raise ValueError(f"data too large for one-shot SHA-256: {len(data)} (max 240)")

        self._send_command(Command.CMD_SHA256, data)
        rsp_id, _seq, out = self._read_response(expected={Response.RSP_SHA256, Response.ERROR})
        if rsp_id == Response.ERROR:
            err = out[0] if out else 0
            raise CryptoError(f"sha256 failed: err=0x{err:02X}")
        if len(out) != 32:
            raise CryptoError(f"sha256 returned {len(out)} bytes, expected 32")
        return out

    def _resolve_curve_id(self, curve: str) -> int:
        if curve == "p256":
            return 0
        if curve == "curve25519":
            return 1
        raise ValueError(f"unknown curve {curve!r}; expected p256|curve25519")

    def ecdsa_sign(self, priv: bytes, msg_hash: bytes, curve: str) -> bytes:
        """Sign `msg_hash` (32 B) with `priv` (32 B). Returns 64-byte signature (r||s)."""
        from feralrf.exceptions import CryptoError

        if len(priv) != 32:
            raise ValueError(f"priv must be 32 bytes, got {len(priv)}")
        if len(msg_hash) != 32:
            raise ValueError(f"hash must be 32 bytes, got {len(msg_hash)}")
        curve_id = self._resolve_curve_id(curve)
        payload = bytes([curve_id]) + priv + msg_hash
        self._send_command(Command.CMD_ECDSA_SIGN, payload)
        rsp_id, _seq, out = self._read_response(expected={Response.RSP_ECDSA_SIG, Response.ERROR})
        if rsp_id == Response.ERROR:
            err = out[0] if out else 0
            if err == 0x05:
                raise CryptoError(f"ecdsa_sign: curve {curve!r} not supported by firmware")
            raise CryptoError(f"ecdsa_sign failed: err=0x{err:02X}")
        if len(out) != 64:
            raise CryptoError(f"ecdsa_sign returned {len(out)} bytes, expected 64")
        return out

    def ecdsa_verify(self, pub: bytes, msg_hash: bytes, sig: bytes, curve: str) -> bool:
        """Verify `sig` for `msg_hash` under `pub`. Returns True/False."""
        from feralrf.exceptions import CryptoError

        if len(msg_hash) != 32:
            raise ValueError(f"hash must be 32 bytes, got {len(msg_hash)}")
        if len(sig) != 64:
            raise ValueError(f"sig must be 64 bytes, got {len(sig)}")
        curve_id = self._resolve_curve_id(curve)
        if curve == "p256" and len(pub) != 64:
            raise ValueError(f"pub must be 64 bytes for p256, got {len(pub)}")
        if curve == "curve25519" and len(pub) != 32:
            raise ValueError(f"pub must be 32 bytes for curve25519, got {len(pub)}")
        payload = bytes([curve_id]) + pub + msg_hash + sig
        self._send_command(Command.CMD_ECDSA_VERIFY, payload)
        rsp_id, _seq, out = self._read_response(
            expected={Response.RSP_ECDSA_VERIFY, Response.ERROR}
        )
        if rsp_id == Response.ERROR:
            err = out[0] if out else 0
            if err == 0x05:
                raise CryptoError(f"ecdsa_verify: curve {curve!r} not supported")
            raise CryptoError(f"ecdsa_verify failed: err=0x{err:02X}")
        return out == b"\x01"

    def ecdh(self, my_priv: bytes, peer_pub: bytes, curve: str) -> bytes:
        """Compute ECDH shared secret. `curve`: 'p256' (peer_pub=64 B) or 'curve25519' (peer_pub=32 B)."""
        from feralrf.exceptions import CryptoError

        if len(my_priv) != 32:
            raise ValueError(f"priv must be 32 bytes, got {len(my_priv)}")
        if curve == "p256":
            curve_id = 0
            if len(peer_pub) != 64:
                raise ValueError(f"peer_pub must be 64 bytes for p256, got {len(peer_pub)}")
        elif curve == "curve25519":
            curve_id = 1
            if len(peer_pub) != 32:
                raise ValueError(f"peer_pub must be 32 bytes for curve25519, got {len(peer_pub)}")
        else:
            raise ValueError(f"unknown curve {curve!r}; expected p256|curve25519")

        payload = bytes([curve_id]) + my_priv + peer_pub
        self._send_command(Command.CMD_ECDH, payload)
        rsp_id, _seq, out = self._read_response(expected={Response.RSP_ECDH, Response.ERROR})
        if rsp_id == Response.ERROR:
            err = out[0] if out else 0
            if err == 0x05:
                raise CryptoError(f"ecdh: curve {curve!r} not supported by firmware")
            raise CryptoError(f"ecdh failed: err=0x{err:02X}")
        if len(out) != 32:
            raise CryptoError(f"ecdh returned {len(out)} bytes, expected 32")
        return out

    def _aes_gcm_op(self, op, key, iv, aad, data, tag_in):
        from feralrf.exceptions import CryptoError

        if len(key) != 16:
            raise ValueError(f"key must be 16 bytes, got {len(key)}")
        if len(iv) != 12:
            raise ValueError(f"iv must be 12 bytes for GCM, got {len(iv)}")
        if op == 1 and len(tag_in) != 16:
            raise ValueError("tag must be 16 bytes")

        header = bytes(
            [
                op,
                *key,
                *iv,
                len(aad) & 0xFF,
                (len(aad) >> 8) & 0xFF,
                len(data) & 0xFF,
                (len(data) >> 8) & 0xFF,
            ]
        )
        payload = header + aad + data + (tag_in if op == 1 else b"")

        self._send_command(Command.CMD_AES_GCM, payload)
        rsp_id, _seq, out = self._read_response(expected={Response.RSP_AES_GCM, Response.ERROR})
        if rsp_id == Response.ERROR:
            err = out[0] if out else 0
            if err == 0x02:
                raise CryptoError("aes_gcm: tag mismatch")
            raise CryptoError(f"aes_gcm failed: err=0x{err:02X}")
        if op == 0:
            return (out[: len(data)], out[len(data) : len(data) + 16])
        return (out, b"")

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
