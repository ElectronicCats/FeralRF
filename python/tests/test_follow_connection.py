"""Unit tests for F8b Track B passive connection follower API."""

import pytest

from feralrf.commands import CommandBuilder


class TestFollowStartBuilder:
    def test_no_filter_yields_six_zero_bytes(self):
        # Wildcard MAC = b"\x00" * 6 (firmware contract: zero-MAC ⇒ no filter)
        out = CommandBuilder.follow_start(target_mac_le=None)
        assert out == b"\x00\x00\x00\x00\x00\x00"

    def test_explicit_mac_passed_through_little_endian(self):
        # MAC "AA:BB:CC:DD:EE:FF" → bytes A8 E6 E8 8A 7D F8 in BLE little-endian
        mac_le = bytes.fromhex("F87D8AE8E6A8")
        out = CommandBuilder.follow_start(target_mac_le=mac_le)
        assert out == mac_le
        assert len(out) == 6

    def test_wrong_length_raises(self):
        with pytest.raises(ValueError):
            CommandBuilder.follow_start(target_mac_le=b"\x01\x02\x03")  # len=3

    def test_seven_bytes_raises(self):
        with pytest.raises(ValueError):
            CommandBuilder.follow_start(target_mac_le=b"\x01" * 7)


class TestFollowStopBuilder:
    def test_empty_payload(self):
        assert CommandBuilder.follow_stop() == b""


class TestLLPacketDataclass:
    def test_construct_with_required_fields(self):
        from feralrf.radio import LLPacket

        pkt = LLPacket(
            direction="M->S",
            channel=10,
            rssi_dbm=-60,
            event_counter=42,
            payload=b"\x03\x05\x12\x00\x60\x01\x00",
            timestamp=1234.5,
        )
        assert pkt.direction == "M->S"
        assert pkt.channel == 10
        assert pkt.rssi_dbm == -60
        assert pkt.event_counter == 42
        assert pkt.payload == b"\x03\x05\x12\x00\x60\x01\x00"
        assert pkt.timestamp == 1234.5

    def test_direction_must_be_known_token(self):
        from feralrf.radio import LLPacket

        # Validation is deliberately *not* enforced in __init__; the firmware
        # is the source of truth. The host preserves whatever string it
        # was given. This test pins that the dataclass is permissive.
        pkt = LLPacket(
            direction="UNKNOWN",
            channel=0,
            rssi_dbm=0,
            event_counter=0,
            payload=b"",
            timestamp=0.0,
        )
        assert pkt.direction == "UNKNOWN"


class TestFollowConnectionAPI:
    """Mocked-serial tests for Radio.follow_connection / read_ll_packets."""

    def _make_radio_with_fake_serial(self, response_frames):
        """Build a Radio with a mocked serial that emits the given frames in order."""
        from unittest.mock import MagicMock

        from feralrf.protocol import build_frame
        from feralrf.radio import Radio

        r = Radio("/dev/null")
        r._serial = MagicMock()
        r._serial.is_open = True
        # Build COBS-encoded responses concatenated into the read stream
        stream = bytearray()
        for cmd_id, seq, payload in response_frames:
            frame = build_frame(cmd_id, seq, payload)
            stream.extend(frame)

        # MagicMock for serial.read(n): return one byte at a time
        idx = [0]

        def fake_read(n=1):
            if idx[0] >= len(stream):
                return b""
            out = stream[idx[0] : idx[0] + n]
            idx[0] += n
            return bytes(out)

        r._serial.read = fake_read
        r._serial.timeout = 1.0
        return r

    def test_follow_connection_with_mac_sends_correct_command(self):
        from feralrf.enums import Response

        r = self._make_radio_with_fake_serial([(Response.ACK, 0, b"")])
        r.follow_connection(target_mac="A8:E6:E8:8A:7D:F8", timeout=1.0)
        # Verify the bytes written to serial included CMD_FOLLOW_START + LE MAC
        write_calls = [c[0][0] for c in r._serial.write.call_args_list]
        assert len(write_calls) == 1
        # The command sent should contain the MAC in LE order (F8 7D 8A E8 E6 A8)
        assert b"\xf8\x7d\x8a\xe8\xe6\xa8" in write_calls[0]

    def test_stop_follow_connection_sends_stop(self):
        from feralrf.enums import Response

        r = self._make_radio_with_fake_serial([(Response.ACK, 0, b"")])
        r.stop_follow_connection(timeout=1.0)
        write_calls = [c[0][0] for c in r._serial.write.call_args_list]
        assert len(write_calls) == 1


class TestReadLLPackets:
    def test_iterator_yields_parsed_packets(self):
        from feralrf.enums import Response
        from feralrf.radio import LLPacket

        # Wire format: [dir:1][ch:1][rssi:1][event:2LE][ll_pdu:N]
        payload1 = (
            b"M"  # direction
            + bytes([10])  # channel
            + bytes([0xC4])  # rssi -60 as signed byte
            + b"\x05\x00"  # event counter 5
            + b"\x02\x09\x05\x00\x04\x00\x12\xd5\x00\x01\x00"  # LL PDU
        )
        # Make the test simple by reusing the helper from prior class
        helper = TestFollowConnectionAPI()
        r = helper._make_radio_with_fake_serial([(Response.LL_PACKET, 0, payload1)])
        pkts = list(r.read_ll_packets(timeout=0.5))
        assert len(pkts) == 1
        assert isinstance(pkts[0], LLPacket)
        assert pkts[0].channel == 10
        assert pkts[0].rssi_dbm == -60
        assert pkts[0].event_counter == 5
        assert pkts[0].direction == "M"
        assert pkts[0].payload.startswith(b"\x02\x09\x05")

    def test_iterator_ends_on_follow_done(self):
        from feralrf.enums import Response

        helper = TestFollowConnectionAPI()
        # Single FOLLOW_DONE frame should end the iterator quietly
        done_payload = b"\x00" + (0).to_bytes(4, "little")  # reason=HOST_STOP, count=0
        r = helper._make_radio_with_fake_serial([(Response.FOLLOW_DONE, 0, done_payload)])
        pkts = list(r.read_ll_packets(timeout=0.5))
        assert pkts == []

    def test_iterator_ends_on_timeout(self):
        helper = TestFollowConnectionAPI()
        r = helper._make_radio_with_fake_serial([])  # nothing to read → timeout
        pkts = list(r.read_ll_packets(timeout=0.1))
        assert pkts == []
