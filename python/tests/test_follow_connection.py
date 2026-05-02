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
