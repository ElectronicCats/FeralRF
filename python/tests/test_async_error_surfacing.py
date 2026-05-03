"""F8f #7b — async error surfacing through read_packets() and _read_response().

Reuses the FakeSerial pattern from test_gatt_api.py; no make_radio fixture.
"""

from typing import List, Optional, Tuple

from feralrf.enums import Response
from feralrf.protocol import build_frame
from feralrf.radio import Radio, RxStreamError


class FakeSerial:
    """Local copy of the FakeSerial from test_gatt_api.py.

    If both files end up needing it, refactor into a shared conftest.py
    fixture in a follow-up — out of scope here.
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
        # build_frame() returns COBS-encoded bytes WITH the trailing 0x00
        # delimiter that Radio._read_response expects (see protocol.py:15).
        self._read_buf.extend(build_frame(cmd_id, seq, payload))


def _radio_with_fake_serial() -> Tuple[Radio, FakeSerial]:
    radio = Radio(port="/dev/null")
    fake = FakeSerial()
    radio._serial = fake  # type: ignore[assignment]
    return radio, fake


def test_read_packets_yields_rx_stream_error_seq_zero():
    """Post-#7a contract: seq=0 async error surfaces as RxStreamError."""
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ERROR, seq=0, payload=bytes([0x06, 0x02]))

    events: List[object] = list(radio.read_packets(timeout=0.1))
    errs = [e for e in events if isinstance(e, RxStreamError)]
    assert len(errs) == 1
    assert errs[0].error_code == 0x06
    assert errs[0].context == 0x02


def test_read_packets_yields_rx_stream_error_seq_0xff_compat():
    """Permanent compat: pre-#7a firmware emits seq=0xFF; surface identically.

    The dual-accept (seq in (0, 0xFF)) is intentionally permanent so a new
    host doesn't silently drop async errors when talking to old firmware.
    """
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ERROR, seq=0xFF, payload=bytes([0x05]))

    events: List[object] = list(radio.read_packets(timeout=0.1))
    errs = [e for e in events if isinstance(e, RxStreamError)]
    assert len(errs) == 1
    assert errs[0].error_code == 0x05
    assert errs[0].context == 0


def test_read_response_returns_seq_0xff_error_when_error_expected():
    """F8f #7 follow-up — synchronous caller waiting for RSP_ERROR must see
    seq=0xFF async errors instead of timing out.

    Original Bundle 1 fix had the async-error bypass fall through into the
    stale-seq filter, which dropped seq=0xFF as `seq != _last_seq`. The
    follow-up returns directly when ERROR is in `expected`, bypassing the
    stale check entirely. Repro from the post-Bundle-2 review.
    """
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.ERROR, seq=0xFF, payload=bytes([0x05]))

    cmd_id, seq, payload = radio._read_response(timeout=0.1, expected={Response.ERROR})
    assert cmd_id == Response.ERROR.value
    assert seq == 0xFF
    assert payload == bytes([0x05])
