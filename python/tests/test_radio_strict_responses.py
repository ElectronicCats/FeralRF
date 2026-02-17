"""
FeralRF - Strict response handling tests for Radio
"""

import pytest

from feralrf.enums import Command, Response
from feralrf.exceptions import ProtocolError
from feralrf.protocol import build_frame
from feralrf.radio import Radio


class FakeSerial:
    def __init__(self, read_stream: bytes):
        self._stream = bytearray(read_stream)
        self.timeout = 1.0
        self.write_timeout = 1.0
        self.is_open = True
        self.written = bytearray()

    def read(self, n: int = 1) -> bytes:
        if not self._stream:
            return b""
        out = self._stream[:n]
        del self._stream[:n]
        return bytes(out)

    def write(self, data: bytes) -> int:
        self.written.extend(data)
        return len(data)

    def flush(self) -> None:
        return None

    def close(self) -> None:
        self.is_open = False


def test_read_response_ignores_echoed_command_frames():
    echoed_command = build_frame(Command.GET_INFO, 0x10, b"")
    ack_response = build_frame(Response.ACK, 0x10, b"")

    radio = Radio(port="dummy")
    radio._serial = FakeSerial(echoed_command + ack_response)

    cmd_id, seq, payload = radio._read_response(timeout=0.1, expected={Response.ACK, Response.ERROR})
    assert cmd_id == Response.ACK
    assert seq == 0x10
    assert payload == b""


def test_init_accepts_ack_then_info(monkeypatch):
    info_payload = bytes([1, 0, 0, 0x01]) + b"FERALRF1"
    read_stream = build_frame(Response.ACK, 0x00, b"")
    read_stream += build_frame(Command.GET_INFO, 0x01, b"")  # echoed frame should be ignored
    read_stream += build_frame(Response.INFO, 0x01, info_payload)
    fake_serial = FakeSerial(read_stream)

    radio = Radio(port="dummy")

    def fake_connect():
        radio._serial = fake_serial

    monkeypatch.setattr(radio, "connect", fake_connect)

    info = radio.init()
    assert info.firmware_version == "1.0.0"
    assert info.capabilities == 0x01
    assert info.serial == b"FERALRF1".hex()


def test_init_fails_on_short_info_payload(monkeypatch):
    short_payload = bytes([1, 0, 0])  # missing capabilities + serial
    read_stream = build_frame(Response.ACK, 0x00, b"")
    read_stream += build_frame(Response.INFO, 0x01, short_payload)
    fake_serial = FakeSerial(read_stream)

    radio = Radio(port="dummy")

    def fake_connect():
        radio._serial = fake_serial

    monkeypatch.setattr(radio, "connect", fake_connect)

    with pytest.raises(ProtocolError, match="INFO payload too short"):
        radio.init()
