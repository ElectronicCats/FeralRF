"""FeralRF — GATT API unit tests (no hardware)."""

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
