"""Unit tests for F8b Track B passive connection follower API."""
import struct
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
