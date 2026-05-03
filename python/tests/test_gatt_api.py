"""FeralRF — GATT API unit tests (no hardware)."""

import pytest

from feralrf.commands import CommandBuilder
from feralrf.enums import Command, Response


def test_command_enum_has_ble_connection_and_gatt_ids():
    # BLE connection
    assert Command.CONNECT == 0x40
    assert Command.DISCONNECT == 0x41
    assert Command.CONN_STATUS == 0x42
    # GATT
    assert Command.GATT_DISCOVER == 0x43
    assert Command.GATT_READ == 0x45
    assert Command.GATT_WRITE == 0x46


def test_response_enum_has_connection_and_gatt_ids():
    # Connection
    assert Response.CONN_RESULT == 0xA0
    assert Response.CONN_STATUS == 0xA1
    # GATT
    assert Response.GATT_SERVICE == 0xA2
    assert Response.GATT_CHAR == 0xA3
    assert Response.GATT_READ_VALUE == 0xA4
    assert Response.GATT_DONE == 0xA5


# --- CommandBuilder payload tests ---


def test_ble_connect_payload_is_addr_le_plus_type():
    addr_le = b"\x01\xEE\xDD\xCC\xBB\xAA"
    assert CommandBuilder.ble_connect(addr_le, addr_type=0) == addr_le + b"\x00"
    assert CommandBuilder.ble_connect(addr_le, addr_type=1) == addr_le + b"\x01"


def test_ble_connect_rejects_wrong_length():
    with pytest.raises(ValueError):
        CommandBuilder.ble_connect(b"\x01\x02\x03", addr_type=0)


def test_ble_disconnect_and_conn_status_are_empty():
    assert CommandBuilder.ble_disconnect() == b""
    assert CommandBuilder.conn_status() == b""


def test_gatt_discover_is_empty():
    assert CommandBuilder.gatt_discover() == b""


def test_gatt_read_payload_is_u16_le_handle():
    assert CommandBuilder.gatt_read(0x002A) == b"\x2A\x00"


def test_gatt_write_payload_is_handle_plus_data():
    assert CommandBuilder.gatt_write(0x002A, b"\xDE\xAD\xBE\xEF") == b"\x2A\x00\xDE\xAD\xBE\xEF"


def test_gatt_write_allows_empty_data():
    assert CommandBuilder.gatt_write(0x0010, b"") == b"\x10\x00"


# --- Dataclass tests ---

from feralrf.radio import (  # noqa: E402
    ConnectionResult,
    ConnectionStatus,
    GattCharacteristic,
    GattDiscoveryResult,
    GattService,
)


def test_connection_result_dataclass():
    r = ConnectionResult(result=0)
    assert r.result == 0
    assert r.is_ok


def test_connection_result_is_ok_false_when_nonzero():
    assert ConnectionResult(result=1).is_ok is False


def test_connection_status_minimum_fields():
    s = ConnectionStatus(connected=True, interval=40, events=3, last_status=0x1400)
    assert s.connected is True
    assert s.interval == 40


def test_gatt_service_fields():
    svc = GattService(start_handle=0x0001, end_handle=0x0005, uuid=b"\x00\x18")
    assert svc.start_handle == 0x0001
    assert svc.end_handle == 0x0005
    assert svc.uuid == b"\x00\x18"


def test_gatt_characteristic_fields():
    ch = GattCharacteristic(handle=0x0002, properties=0x02, value_handle=0x0003, uuid=b"\x00\x2A")
    assert ch.handle == 0x0002
    assert ch.properties == 0x02
    assert ch.value_handle == 0x0003


def test_gatt_discovery_result_is_empty_by_default():
    res = GattDiscoveryResult(services=[], characteristics=[], status=0)
    assert res.services == []
    assert res.characteristics == []
    assert res.status == 0


# --- Radio method tests with FakeSerial ---

import struct  # noqa: E402
from typing import List, Optional, Tuple  # noqa: E402

from feralrf.exceptions import CommandError  # noqa: E402
from feralrf.protocol import build_frame, cobs_decode, parse_frame  # noqa: E402
from feralrf.radio import Radio  # noqa: E402


class FakeSerial:
    """Minimal serial stand-in for Radio unit tests.

    Captures frames written by the Radio and plays back pre-canned
    response frames on read. Each call to `queue_response()` adds one
    COBS-delimited frame to the read buffer.
    """

    def __init__(self) -> None:
        self.is_open = True
        self.written: bytearray = bytearray()
        self._read_buf: bytearray = bytearray()
        self.timeout: Optional[float] = None

    def write(self, data: bytes) -> int:
        self.written.extend(data)
        return len(data)

    def flush(self) -> None:
        pass

    def read(self, n: int = 1) -> bytes:
        if not self._read_buf:
            return b""
        out = bytes(self._read_buf[:n])
        del self._read_buf[:n]
        return out

    def reset_input_buffer(self) -> None:
        self._read_buf.clear()

    def reset_output_buffer(self) -> None:
        self.written.clear()

    def close(self) -> None:
        self.is_open = False

    def queue_response(self, cmd_id: int, seq: int, payload: bytes = b"") -> None:
        self._read_buf.extend(build_frame(cmd_id, seq, payload))

    def written_frames(self) -> List[Tuple[int, int, bytes]]:
        frames: List[Tuple[int, int, bytes]] = []
        buf = bytearray()
        for b in self.written:
            if b == 0x00:
                if buf:
                    decoded = cobs_decode(bytes(buf))
                    frames.append(parse_frame(decoded))
                buf = bytearray()
            else:
                buf.append(b)
        return frames


def _radio_with_fake_serial() -> Tuple[Radio, FakeSerial]:
    radio = Radio(port="/dev/null")
    fake = FakeSerial()
    radio._serial = fake  # type: ignore[assignment]
    return radio, fake


def test_ble_connect_sends_correct_frame_and_parses_result():
    radio, fake = _radio_with_fake_serial()
    addr_le = b"\x01\xEE\xDD\xCC\xBB\xAA"

    fake.queue_response(Response.CONN_RESULT, seq=0, payload=b"\x00")

    result = radio.ble_connect(addr_le, addr_type=1, timeout=1.0)

    assert isinstance(result, ConnectionResult)
    assert result.is_ok
    frames = fake.written_frames()
    assert len(frames) == 1
    cmd_id, _seq, payload = frames[0]
    assert cmd_id == Command.CONNECT
    assert payload == addr_le + b"\x01"


def test_ble_connect_returns_nonzero_on_timeout_code():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.CONN_RESULT, seq=0, payload=b"\x01")
    result = radio.ble_connect(b"\x01\xEE\xDD\xCC\xBB\xAA", addr_type=1, timeout=1.0)
    assert result.result == 1
    assert not result.is_ok


def test_ble_disconnect_sends_cmd_disconnect_and_accepts_ack():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ACK, seq=0)
    radio.ble_disconnect(timeout=1.0)
    frames = fake.written_frames()
    assert len(frames) == 1
    assert frames[0][0] == Command.DISCONNECT
    assert frames[0][2] == b""


def test_conn_status_parses_short_payload():
    radio, fake = _radio_with_fake_serial()
    payload = (
        bytes([1])
        + struct.pack("<H", 40)
        + b"\x00\x00"
        + struct.pack("<H", 5)
        + struct.pack("<H", 0x1400)
    )
    assert len(payload) == 9
    fake.queue_response(Response.CONN_STATUS, seq=0, payload=payload)

    status = radio.conn_status(timeout=1.0)
    assert status.connected is True
    assert status.interval == 40
    assert status.events == 5
    assert status.last_status == 0x1400
    assert status.tx_done is None
    assert status.att_state is None


def test_conn_status_parses_extended_payload():
    radio, fake = _radio_with_fake_serial()
    payload = (
        bytes([1])
        + struct.pack("<H", 40)
        + b"\x00\x00"
        + struct.pack("<H", 5)
        + struct.pack("<H", 0x1400)
        + struct.pack("<H", 7)
        + bytes([3])
        + struct.pack("<H", 12)
    )
    assert len(payload) == 14
    fake.queue_response(Response.CONN_STATUS, seq=0, payload=payload)

    status = radio.conn_status(timeout=1.0)
    assert status.tx_done == 7
    assert status.att_state == 3
    assert status.total_rx == 12


def test_gatt_discover_collects_services_and_chars_until_done():
    radio, fake = _radio_with_fake_serial()

    # Firmware echoes the request's seq on every stream frame
    # (s_gatt_seq = seq). First command uses seq=0.
    fake.queue_response(Response.ACK, seq=0)
    fake.queue_response(
        Response.GATT_SERVICE,
        seq=0,
        payload=struct.pack("<HH", 0x0001, 0x0005) + b"\x00\x18",
    )
    fake.queue_response(
        Response.GATT_CHAR,
        seq=0,
        payload=struct.pack("<HBH", 0x0002, 0x02, 0x0003) + b"\x00\x2A",
    )
    fake.queue_response(
        Response.GATT_SERVICE,
        seq=0,
        payload=struct.pack("<HH", 0x0010, 0x0014) + b"\x0F\x18",
    )
    fake.queue_response(
        Response.GATT_CHAR,
        seq=0,
        payload=struct.pack("<HBH", 0x0011, 0x10, 0x0012) + b"\x19\x2A",
    )
    fake.queue_response(Response.GATT_DONE, seq=0, payload=b"\x00")

    result = radio.gatt_discover(timeout=5.0)

    assert isinstance(result, GattDiscoveryResult)
    assert len(result.services) == 2
    assert len(result.characteristics) == 2
    assert result.services[0].start_handle == 0x0001
    assert result.services[0].uuid == b"\x00\x18"
    assert result.characteristics[0].properties == 0x02
    assert result.status == 0


def test_gatt_discover_raises_on_error_before_done():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ERROR, seq=0, payload=b"\x05")
    with pytest.raises(CommandError):
        radio.gatt_discover(timeout=1.0)


def test_gatt_read_returns_value_bytes():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ACK, seq=0)
    fake.queue_response(
        Response.GATT_READ_VALUE,
        seq=0,
        payload=struct.pack("<H", 0x0003) + b"Device42",
    )
    value = radio.gatt_read(0x0003, timeout=3.0)
    assert value == b"Device42"


def test_gatt_write_returns_status_byte():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ACK, seq=0)
    fake.queue_response(Response.GATT_DONE, seq=0, payload=b"\x00")
    status = radio.gatt_write(0x0010, b"\xDE\xAD", timeout=3.0)
    assert status == 0


def test_gatt_methods_are_declared_stable():
    assert "ble_connect" in Radio.STABLE_METHODS
    assert "ble_disconnect" in Radio.STABLE_METHODS
    assert "conn_status" in Radio.STABLE_METHODS
    assert "gatt_discover" in Radio.STABLE_METHODS
    assert "gatt_read" in Radio.STABLE_METHODS
    assert "gatt_write" in Radio.STABLE_METHODS
    assert "gatt_discovery" not in Radio.PENDING_FEATURES


# --- F8c protocol IDs ---


def test_f8c_command_ids_are_assigned():
    assert Command.GATT_EXCHANGE_MTU == 0x4A
    assert Command.GATT_READ_BY_UUID == 0x4B


def test_f8c_response_ids_are_assigned():
    assert Response.GATT_MTU == 0xB0
    assert Response.GATT_ATTRIBUTE == 0xB1
    assert Response.DISCONNECTED == 0xB2


def test_gatt_exchange_mtu_payload_is_u16_le_client_mtu():
    assert CommandBuilder.gatt_exchange_mtu(client_mtu=23) == b"\x17\x00"
    assert CommandBuilder.gatt_exchange_mtu(client_mtu=247) == b"\xF7\x00"


def test_gatt_exchange_mtu_rejects_too_small():
    with pytest.raises(ValueError):
        CommandBuilder.gatt_exchange_mtu(client_mtu=22)  # MTU must be >= 23 per spec


def test_gatt_read_by_uuid_uuid16_payload_is_start_end_uuid_le():
    payload = CommandBuilder.gatt_read_by_uuid(start=0x0001, end=0xFFFF, uuid=0x2A00)
    assert payload == b"\x01\x00\xFF\xFF\x00\x2A"


def test_gatt_read_by_uuid_rejects_inverted_range():
    with pytest.raises(ValueError):
        CommandBuilder.gatt_read_by_uuid(start=0x0010, end=0x0001, uuid=0x2A00)


def test_gatt_read_by_uuid_rejects_uuid_above_16_bits():
    with pytest.raises(ValueError):
        CommandBuilder.gatt_read_by_uuid(start=0x0001, end=0xFFFF, uuid=0x12A00)
