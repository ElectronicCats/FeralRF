"""Unit tests for F8b Track A — GATT notifications host-side API.

Hardware-free; mocks the Radio _send_command + _read_response transport.
"""

from unittest.mock import MagicMock

import pytest

from feralrf import Radio
from feralrf.enums import Command, Response
from feralrf.exceptions import CommandError


@pytest.fixture
def radio_mock():
    """Radio instance with mocked transport methods."""
    r = Radio()
    r._send_command = MagicMock()
    r._read_response = MagicMock()
    r._serial = MagicMock()
    r._serial.is_open = True
    return r


def test_gatt_subscribe_writes_correct_command(radio_mock):
    """gatt_subscribe(212, enable=True) must send CMD_GATT_SUBSCRIBE
    with payload handle_le[2] + enable[1] + indicate[1]."""
    radio_mock._read_response.return_value = (Response.ACK, 0, b"")

    radio_mock.gatt_subscribe(handle=212, enable=True)

    # Verify _send_command call
    call_args = radio_mock._send_command.call_args
    cmd_id, payload = call_args[0]
    assert cmd_id == Command.GATT_SUBSCRIBE
    assert payload == b"\xd4\x00\x01\x00"  # 212 LE, enable=1, indicate=0


def test_gatt_subscribe_indicate_sets_indicate_byte(radio_mock):
    """gatt_subscribe(212, indicate=True) sets the indicate flag."""
    radio_mock._read_response.return_value = (Response.ACK, 0, b"")

    radio_mock.gatt_subscribe(handle=212, enable=True, indicate=True)

    cmd_id, payload = radio_mock._send_command.call_args[0]
    assert payload == b"\xd4\x00\x01\x01"


def test_gatt_subscribe_disable(radio_mock):
    """gatt_subscribe(212, enable=False) sets enable=0."""
    radio_mock._read_response.return_value = (Response.ACK, 0, b"")

    radio_mock.gatt_subscribe(handle=212, enable=False)

    cmd_id, payload = radio_mock._send_command.call_args[0]
    assert payload[2] == 0


def test_gatt_subscribe_raises_on_error_response(radio_mock):
    """If firmware returns RSP_ERROR, gatt_subscribe raises CommandError."""
    radio_mock._read_response.return_value = (Response.ERROR, 0, b"\x05")  # ERR_INVALID_STATE

    with pytest.raises(CommandError):
        radio_mock.gatt_subscribe(handle=212, enable=True)


def test_gatt_notification_dataclass_fields():
    """GattNotification has handle, value, timestamp."""
    from feralrf.radio import GattNotification

    n = GattNotification(handle=212, value=b"\x01\x02\x03", timestamp=123.456)
    assert n.handle == 212
    assert n.value == b"\x01\x02\x03"
    assert n.timestamp == 123.456


def test_gatt_notification_repr():
    """GattNotification has a useful repr including handle and hex value."""
    from feralrf.radio import GattNotification

    n = GattNotification(handle=0xD4, value=b"\xab\xcd", timestamp=0.0)
    s = repr(n)
    assert "212" in s or "0xd4" in s.lower() or "GattNotification" in s
    assert "abcd" in s.lower() or "ab cd" in s.lower() or "b'\\xab\\xcd'" in s
