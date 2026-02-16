"""
FeralRF - Protocol tests
"""

import pytest

from feralrf.enums import Command
from feralrf.protocol import build_frame, cobs_decode, cobs_encode, crc16_ccitt, parse_frame


class TestCOBS:
    """COBS encoding/decoding tests"""

    def test_encode_empty(self):
        """Test encoding empty data"""
        encoded = cobs_encode(b"")
        assert 0x00 not in encoded[1:-1]

    def test_encode_no_zeros(self):
        """Test encoding data without zeros"""
        data = b"\x01\x02\x03\x04"
        encoded = cobs_encode(data)
        assert 0x00 not in encoded[1:-1]

    def test_encode_with_zeros(self):
        """Test encoding data with zeros"""
        data = b"\x01\x00\x02\x00\x03"
        encoded = cobs_encode(data)
        assert 0x00 not in encoded[1:-1]

    def test_decode_empty(self):
        """Test decoding empty data"""
        decoded = cobs_decode(b"\x01\x00")
        assert decoded == b""

    def test_roundtrip(self):
        """Test encode/decode roundtrip"""
        test_cases = [
            b"",
            b"\x01",
            b"\x01\x02\x03",
            b"\x01\x00\x02",  # Zero in middle
            b"\x00\x01",  # Zero at start
            b"\x01\x02\x00",  # Zero at end
            bytes(range(1, 256)),  # All non-zero bytes
            b"\x00" * 2,  # Multiple zeros
            b"\xAA" * 100,  # Long non-zero
        ]

        for data in test_cases:
            encoded = cobs_encode(data)
            decoded = cobs_decode(encoded)
            assert decoded == data, f"Roundtrip failed for {data.hex()}"


class TestCRC16:
    """CRC-16-CCITT tests"""

    def test_crc_empty(self):
        """Test CRC of empty data"""
        crc = crc16_ccitt(b"")
        assert crc == 0xFFFF

    def test_crc_known_values(self):
        """Test CRC against known values"""
        # "123456789" should give 0x29B1
        crc = crc16_ccitt(b"123456789")
        assert crc == 0x29B1

    def test_crc_consistency(self):
        """Test CRC is consistent"""
        data = b"\x01\x02\x03\x04\x05"
        crc1 = crc16_ccitt(data)
        crc2 = crc16_ccitt(data)
        assert crc1 == crc2


class TestFrame:
    """Frame building and parsing tests"""

    def test_build_frame_empty(self):
        """Test building frame with empty payload"""
        frame = build_frame(Command.GET_INFO, 0x00)
        assert len(frame) > 0
        assert 0x00 not in frame[1:-1]  # No zeros except delimiters

    def test_build_frame_with_payload(self):
        """Test building frame with payload"""
        frame = build_frame(Command.TX_RAW, 0x01, b"\xAA\xBB\xCC")
        assert len(frame) > 0
        assert 0x00 not in frame[1:-1]

    def test_parse_frame_roundtrip(self):
        """Test frame build/parse roundtrip"""
        cmd_id = Command.SET_CHANNEL
        seq = 0x42
        payload = b"\x25"  # Channel 37

        frame = build_frame(cmd_id, seq, payload)

        # Decode COBS to get raw frame
        from feralrf.protocol import cobs_decode

        raw_frame = cobs_decode(frame)

        parsed_cmd, parsed_seq, parsed_payload = parse_frame(raw_frame)

        assert parsed_cmd == cmd_id
        assert parsed_seq == seq
        assert parsed_payload == payload

    def test_parse_frame_invalid_crc(self):
        """Test parsing frame with invalid CRC"""
        # Build a frame and corrupt the CRC
        from feralrf.protocol import cobs_decode

        frame = build_frame(Command.GET_INFO, 0x00)
        raw_frame = bytearray(cobs_decode(frame))

        # Corrupt CRC
        raw_frame[-1] ^= 0xFF

        with pytest.raises(ValueError, match="CRC"):
            parse_frame(bytes(raw_frame))

    def test_parse_frame_too_short(self):
        """Test parsing frame that's too short"""
        with pytest.raises(ValueError, match="too short"):
            parse_frame(b"\x01\x02")
