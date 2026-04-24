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
