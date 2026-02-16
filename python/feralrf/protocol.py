"""
FeralRF - Protocol implementation (COBS + CRC16)
"""

import struct
from typing import Tuple

from cobs import cobs as _cobs


def cobs_encode(data: bytes) -> bytes:
    """Encode data using COBS with trailing 0x00 delimiter"""
    if not data:
        return b"\x00"
    return _cobs.encode(data) + b"\x00"


def cobs_decode(data: bytes) -> bytes:
    """Decode COBS-encoded data (with trailing 0x00 delimiter)"""
    if not data:
        return b""
    # Remove trailing delimiter if present
    if data[-1:] == b"\x00":
        data = data[:-1]
    if not data:
        return b""
    return _cobs.decode(data)


def crc16_ccitt(data: bytes, initial: int = 0xFFFF) -> int:
    """
    Calculate CRC-16-CCITT
    """
    crc = initial
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc <<= 1
            crc &= 0xFFFF
    return crc


def build_frame(cmd_id: int, seq: int, payload: bytes = b"") -> bytes:
    """
    Build a complete COBS-encoded frame

    Frame format (pre-COBS):
    [CMD_ID:1][SEQ:1][LEN:2][PAYLOAD:N][CRC16:2]
    """
    # Build frame without COBS encoding
    length = len(payload)
    frame = struct.pack("<BBH", cmd_id, seq, length)
    frame += payload

    # Calculate CRC
    crc = crc16_ccitt(frame)
    frame += struct.pack("<H", crc)

    # COBS encode
    return cobs_encode(frame)


def parse_frame(data: bytes) -> Tuple[int, int, bytes]:
    """
    Parse a COBS-decoded frame

    Returns: (cmd_id, seq, payload)
    Raises: ValueError on invalid frame
    """
    if len(data) < 6:  # Minimum: CMD + SEQ + LEN + CRC
        raise ValueError("Frame too short")

    # Verify CRC
    frame_crc = struct.unpack("<H", data[-2:])[0]
    calc_crc = crc16_ccitt(data[:-2])
    if frame_crc != calc_crc:
        raise ValueError(f"CRC mismatch: expected {frame_crc:04x}, got {calc_crc:04x}")

    # Parse header
    cmd_id, seq, length = struct.unpack("<BBH", data[:4])

    # Extract payload
    payload = data[4:-2]
    if len(payload) != length:
        raise ValueError(f"Payload length mismatch: expected {length}, got {len(payload)}")

    return cmd_id, seq, payload
