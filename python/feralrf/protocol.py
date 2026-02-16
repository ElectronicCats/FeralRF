"""
FeralRF - Protocol implementation (COBS + CRC16)
"""

import struct
from typing import Tuple


def cobs_encode(data: bytes) -> bytes:
    """
    Encode data using COBS (Consistent Overhead Byte Stuffing)
    """
    if not data:
        return b'\x01\x00'

    result = bytearray()
    search_start = 0
    zero_idx = 0

    while search_start < len(data):
        # Find next zero byte
        try:
            zero_idx = data.index(0, search_start)
            block_len = zero_idx - search_start
        except ValueError:
            # No more zeros
            block_len = len(data) - search_start
            zero_idx = len(data)

        # Check for blocks > 254 bytes
        while block_len > 254:
            result.append(0xFF)
            result.extend(data[search_start:search_start + 254])
            search_start += 254
            block_len -= 254

        result.append(block_len + 1)
        result.extend(data[search_start:zero_idx])
        search_start = zero_idx + 1

    result.append(0x00)  # Frame delimiter
    return bytes(result)


def cobs_decode(data: bytes) -> bytes:
    """
    Decode COBS-encoded data
    """
    if not data:
        return b''

    result = bytearray()
    idx = 0

    while idx < len(data):
        block_len = data[idx]
        if block_len == 0:
            break  # End of frame

        idx += 1
        block_len -= 1
        result.extend(data[idx:idx + block_len])

        if block_len < 254 and idx + block_len < len(data):
            result.append(0)  # Add the implicit zero

        idx += block_len

    return bytes(result)


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


def build_frame(cmd_id: int, seq: int, payload: bytes = b'') -> bytes:
    """
    Build a complete COBS-encoded frame

    Frame format (pre-COBS):
    [CMD_ID:1][SEQ:1][LEN:2][PAYLOAD:N][CRC16:2]
    """
    # Build frame without COBS encoding
    length = len(payload)
    frame = struct.pack('<BBH', cmd_id, seq, length)
    frame += payload

    # Calculate CRC
    crc = crc16_ccitt(frame)
    frame += struct.pack('<H', crc)

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
    frame_crc = struct.unpack('<H', data[-2:])[0]
    calc_crc = crc16_ccitt(data[:-2])
    if frame_crc != calc_crc:
        raise ValueError(f"CRC mismatch: expected {frame_crc:04x}, got {calc_crc:04x}")

    # Parse header
    cmd_id, seq, length = struct.unpack('<BBH', data[:4])

    # Extract payload
    payload = data[4:-2]
    if len(payload) != length:
        raise ValueError(f"Payload length mismatch: expected {length}, got {len(payload)}")

    return cmd_id, seq, payload
