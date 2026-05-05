"""F20.a.1 — unit tests for Radio.serve_gatt + CommandBuilder.gatt_serve_table."""

from typing import List, Optional, Tuple

import pytest

from feralrf.commands import CommandBuilder
from feralrf.enums import Command, Response
from feralrf.protocol import build_frame, cobs_decode, parse_frame
from feralrf.radio import Radio


class TestGattServeTablePayload:
    def test_empty_payload(self):
        assert CommandBuilder.gatt_serve_table() == b""


class FakeSerial:
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


class TestRadioServeGatt:
    def test_dispatch_correct_command(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        radio.serve_gatt()
        frames = fake.written_frames()
        assert len(frames) == 1
        cmd_id, _seq, payload = frames[0]
        assert cmd_id == Command.GATT_SERVE_TABLE
        assert payload == b""

    def test_warns_when_table_arg_passed(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        with pytest.warns(UserWarning, match="not yet supported"):
            radio.serve_gatt(table=[("dummy",)])

    def test_seq_advances_after_call(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        seq_before = radio._seq
        radio.serve_gatt()
        assert radio._seq == ((seq_before + 1) & 0xFF)
