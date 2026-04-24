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
