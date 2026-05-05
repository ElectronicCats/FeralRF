"""F21 — unit tests for Radio.advertise_* methods + CommandBuilder.ble_adv_legacy.

Verifies wire format byte layout and Radio class dispatch via FakeSerial.
NO hardware required.
"""

from typing import List, Optional, Tuple

import pytest

from feralrf.commands import CommandBuilder
from feralrf.enums import Command, Response
from feralrf.protocol import build_frame, cobs_decode, parse_frame
from feralrf.radio import Radio


class TestBleAdvLegacyPayload:
    """CommandBuilder.ble_adv_legacy wire-format tests."""

    def test_layout_adv_ind(self):
        p = CommandBuilder.ble_adv_legacy(
            pdu_type=0x0,
            adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            adv_data=b"HELLO",
            scan_rsp_data=b"WORLD",
        )
        assert len(p) == 26
        assert p[0] == 0x0
        assert p[1] == 0x1  # addr_type random (default)
        assert p[2:8] == b"\x06\x05\x04\x03\x02\x01"
        assert p[8] == 37
        assert p[9] == 0x00
        assert p[10] == 0x32  # count=50 LE
        assert p[11] == 0x00
        assert p[12] == 0x10  # interval_units=16 LE
        assert p[13] == 0x00
        assert p[14] == 5  # adv_data_len
        assert p[15:20] == b"HELLO"
        assert p[20] == 5  # scan_rsp_len
        assert p[21:26] == b"WORLD"

    def test_layout_adv_direct(self):
        p = CommandBuilder.ble_adv_legacy(
            pdu_type=0x1,
            adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            init_addr_le=b"\xfe\xee\xdd\xcc\xbb\xaa",
        )
        assert len(p) == 21
        assert p[0] == 0x1
        assert p[14] == 0x1  # init_addr_type random (default)
        assert p[15:21] == b"\xfe\xee\xdd\xcc\xbb\xaa"

    def test_layout_adv_scan_ind(self):
        p = CommandBuilder.ble_adv_legacy(
            pdu_type=0x6,
            adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
            adv_data=b"X",
            scan_rsp_data=b"Y",
        )
        assert p[0] == 0x6
        assert p[14] == 1
        assert p[15:16] == b"X"
        assert p[16] == 1
        assert p[17:18] == b"Y"

    def test_rejects_invalid_pdu_type(self):
        with pytest.raises(ValueError, match="pdu_type"):
            CommandBuilder.ble_adv_legacy(pdu_type=0x2, adv_addr_le=b"\x06\x05\x04\x03\x02\x01")

    def test_rejects_invalid_channel(self):
        with pytest.raises(ValueError, match="channel"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0, adv_addr_le=b"\x06\x05\x04\x03\x02\x01", channel=36
            )

    def test_rejects_invalid_power(self):
        with pytest.raises(ValueError, match="power_dbm"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0, adv_addr_le=b"\x06\x05\x04\x03\x02\x01", power_dbm=25
            )

    def test_rejects_oversized_adv_data(self):
        with pytest.raises(ValueError, match="adv_data"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0,
                adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
                adv_data=b"\x00" * 32,
            )

    def test_rejects_oversized_scan_rsp(self):
        with pytest.raises(ValueError, match="scan_rsp_data"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0,
                adv_addr_le=b"\x06\x05\x04\x03\x02\x01",
                scan_rsp_data=b"\x00" * 32,
            )

    def test_direct_requires_init_addr(self):
        with pytest.raises(ValueError, match="init_addr_le"):
            CommandBuilder.ble_adv_legacy(pdu_type=0x1, adv_addr_le=b"\x06\x05\x04\x03\x02\x01")

    def test_rejects_invalid_count(self):
        with pytest.raises(ValueError, match="count"):
            CommandBuilder.ble_adv_legacy(
                pdu_type=0x0, adv_addr_le=b"\x06\x05\x04\x03\x02\x01", count=0
            )


class FakeSerial:
    """Same FakeSerial pattern as test_gatt_api.py."""

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


class TestRadioAdvertiseMethods:
    """Verify Radio.advertise_* methods send the correct CMD_BLE_ADV_LEGACY frame."""

    def test_advertise_ind_dispatches_pdu_type_0(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        radio.advertise_ind(
            payload=b"\x02\x01\x06",
            target_addr="DE:AD:BE:EF:CA:FE",
            count=5,
        )
        frames = fake.written_frames()
        assert len(frames) == 1
        cmd_id, _seq, payload = frames[0]
        assert cmd_id == Command.BLE_ADV_LEGACY
        assert payload[0] == 0x0
        assert payload[2:8] == b"\xfe\xca\xef\xbe\xad\xde"

    def test_advertise_direct_dispatches_pdu_type_1(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        radio.advertise_direct(
            target_addr="01:02:03:04:05:06",
            init_addr="aa:bb:cc:dd:ee:ff",
            count=3,
        )
        frames = fake.written_frames()
        cmd_id, _, payload = frames[0]
        assert cmd_id == Command.BLE_ADV_LEGACY
        assert payload[0] == 0x1
        assert payload[2:8] == b"\x06\x05\x04\x03\x02\x01"
        assert payload[15:21] == b"\xff\xee\xdd\xcc\xbb\xaa"

    def test_advertise_scan_ind_dispatches_pdu_type_6(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        radio.advertise_scan_ind(
            payload=b"\x02\x01\x06",
            target_addr="DE:AD:BE:EF:CA:FE",
            count=5,
        )
        frames = fake.written_frames()
        cmd_id, _, payload = frames[0]
        assert cmd_id == Command.BLE_ADV_LEGACY
        assert payload[0] == 0x6

    def test_advertise_direct_high_duty_uses_3750us(self):
        radio, fake = _radio_with_fake_serial()
        fake.queue_response(Response.ACK, seq=radio._seq)
        radio.advertise_direct(
            target_addr="01:02:03:04:05:06",
            init_addr="aa:bb:cc:dd:ee:ff",
            mode="high",
            count=3,
        )
        _, _, payload = fake.written_frames()[0]
        interval_units = payload[12] | (payload[13] << 8)
        assert interval_units == 6  # 3750us / 625 = 6
