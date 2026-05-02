"""BLE LL / ATT PDU parser for the F8b Track B connection follower.

Decodes raw LL data PDUs (as captured by the firmware follower and emitted
via RSP_LL_PACKET) into structured form, and exports captures to pcap-NG
for Wireshark inspection. Pure Python, no firmware dependency.

References:
  - Bluetooth Core Spec 5.4 Vol 6 Part B §2.4 (Data Channel PDU)
  - Bluetooth Core Spec 5.4 Vol 3 Part F §3.4 (ATT Protocol)
"""
import struct
from dataclasses import dataclass
from enum import IntEnum
from typing import List, Optional


class LLPduKind(IntEnum):
    """LLID field of LL header (BT Core Spec Vol 6 Part B §2.4.2 Table 2.1)."""

    RESERVED = 0
    DATA_CONT = 1  # L2CAP continuation or empty PDU
    DATA_START = 2  # L2CAP start of complete frame
    CONTROL = 3  # LL Control PDU


# LL Control PDU opcodes (BT Core Spec Vol 6 Part B §2.4.2 Table 2.20)
LL_OPCODE_NAMES = {
    0x00: "LL_CONNECTION_UPDATE_IND",
    0x01: "LL_CHANNEL_MAP_IND",
    0x02: "LL_TERMINATE_IND",
    0x03: "LL_ENC_REQ",
    0x04: "LL_ENC_RSP",
    0x05: "LL_START_ENC_REQ",
    0x06: "LL_START_ENC_RSP",
    0x07: "LL_UNKNOWN_RSP",
    0x08: "LL_FEATURE_REQ",
    0x09: "LL_FEATURE_RSP",
    0x0A: "LL_PAUSE_ENC_REQ",
    0x0B: "LL_PAUSE_ENC_RSP",
    0x0C: "LL_VERSION_IND",
    0x0D: "LL_REJECT_IND",
    0x0E: "LL_SLAVE_FEATURE_REQ",
    0x0F: "LL_CONNECTION_PARAM_REQ",
    0x10: "LL_CONNECTION_PARAM_RSP",
    0x11: "LL_REJECT_EXT_IND",
    0x12: "LL_PING_REQ",
    0x13: "LL_PING_RSP",
    0x14: "LL_LENGTH_REQ",
    0x15: "LL_LENGTH_RSP",
    0x16: "LL_PHY_REQ",
    0x17: "LL_PHY_RSP",
    0x18: "LL_PHY_UPDATE_IND",
    0x19: "LL_MIN_USED_CHANNELS_IND",
}


@dataclass
class LLPdu:
    """Decoded LL Data Channel PDU."""

    kind: LLPduKind
    length: int
    payload: bytes  # bytes after the 2-byte LL header
    opcode: Optional[int] = None  # LL control opcode if kind==CONTROL
    opcode_name: Optional[str] = None
    truncated: bool = False  # length field exceeded available bytes


def parse_ll_pdu(raw: bytes) -> Optional[LLPdu]:
    """Decode an LL Data Channel PDU.

    Args:
        raw: bytes starting at the LL header (LLID byte). Does NOT include
            the access address or CRC — those are stripped by the firmware
            before emit.

    Returns:
        LLPdu, or None if the input is too short to even contain the header.
    """
    if len(raw) < 2:
        return None
    llid = raw[0] & 0x03
    length = raw[1]
    payload = raw[2 : 2 + length]
    truncated = len(payload) < length

    kind = LLPduKind(llid)
    pdu = LLPdu(kind=kind, length=length, payload=payload, truncated=truncated)

    if kind == LLPduKind.CONTROL and len(payload) >= 1:
        pdu.opcode = payload[0]
        pdu.opcode_name = LL_OPCODE_NAMES.get(
            pdu.opcode, f"LL_RFU_{pdu.opcode:#04x}"
        )

    return pdu


# ATT opcodes (BT Core Spec Vol 3 Part F §3.4.8 Table 3.37)
ATT_OPCODE_NAMES = {
    0x01: "ATT_ERROR_RSP",
    0x02: "ATT_EXCHANGE_MTU_REQ",
    0x03: "ATT_EXCHANGE_MTU_RSP",
    0x04: "ATT_FIND_INFO_REQ",
    0x05: "ATT_FIND_INFO_RSP",
    0x06: "ATT_FIND_BY_TYPE_VALUE_REQ",
    0x07: "ATT_FIND_BY_TYPE_VALUE_RSP",
    0x08: "ATT_READ_BY_TYPE_REQ",
    0x09: "ATT_READ_BY_TYPE_RSP",
    0x0A: "ATT_READ_REQ",
    0x0B: "ATT_READ_RSP",
    0x0C: "ATT_READ_BLOB_REQ",
    0x0D: "ATT_READ_BLOB_RSP",
    0x0E: "ATT_READ_MULTIPLE_REQ",
    0x0F: "ATT_READ_MULTIPLE_RSP",
    0x10: "ATT_READ_BY_GROUP_TYPE_REQ",
    0x11: "ATT_READ_BY_GROUP_TYPE_RSP",
    0x12: "ATT_WRITE_REQ",
    0x13: "ATT_WRITE_RSP",
    0x16: "ATT_PREPARE_WRITE_REQ",
    0x17: "ATT_PREPARE_WRITE_RSP",
    0x18: "ATT_EXECUTE_WRITE_REQ",
    0x19: "ATT_EXECUTE_WRITE_RSP",
    0x1B: "ATT_HANDLE_VALUE_NTF",
    0x1D: "ATT_HANDLE_VALUE_IND",
    0x1E: "ATT_HANDLE_VALUE_CFM",
    0x52: "ATT_WRITE_CMD",
    0xD2: "ATT_SIGNED_WRITE_CMD",
}


@dataclass
class AttPdu:
    """Decoded ATT Protocol PDU."""

    opcode: int
    opcode_name: str
    handle: Optional[int] = None
    value: Optional[bytes] = None


def parse_att_pdu(raw: bytes) -> Optional[AttPdu]:
    """Decode an ATT PDU.

    Args:
        raw: bytes starting at the ATT opcode (after L2CAP header is stripped).

    Returns:
        AttPdu with handle/value populated when the opcode has them, or None
        if the input is empty.
    """
    if len(raw) < 1:
        return None
    opcode = raw[0]
    name = ATT_OPCODE_NAMES.get(opcode, f"ATT_RFU_{opcode:#04x}")
    pdu = AttPdu(opcode=opcode, opcode_name=name)

    # Opcodes that carry [handle:2LE] right after the opcode byte
    has_handle = opcode in {0x0A, 0x0B, 0x0C, 0x0D, 0x12, 0x16, 0x1B, 0x1D, 0x52, 0xD2}
    if has_handle and len(raw) >= 3:
        pdu.handle = int.from_bytes(raw[1:3], "little")
        pdu.value = raw[3:] if len(raw) > 3 else b""
    return pdu


# Wireshark LINKTYPE_BLUETOOTH_LE_LL = 251 expects bare LL PDUs
# (without access address). Wireshark dissects it with the BLE LL dissector.
LINKTYPE_BLUETOOTH_LE_LL = 251


def _block(block_type: int, body: bytes) -> bytes:
    """Build a pcap-NG block with proper length and 4-byte alignment."""
    # Pad body to 4-byte boundary
    pad = (4 - (len(body) % 4)) % 4
    body_padded = body + (b"\x00" * pad)
    # Block: [type:4][len:4][body+pad:N][len:4]
    total_len = 12 + len(body_padded)
    return (
        struct.pack("<II", block_type, total_len)
        + body_padded
        + struct.pack("<I", total_len)
    )


def export_pcap(packets: List["LLPacket"], filename: str) -> None:
    """Write captured LL packets to a pcap-NG file.

    The file uses LINKTYPE_BLUETOOTH_LE_LL (251). Wireshark will dissect each
    packet's payload as a BLE LL PDU.

    Args:
        packets: list of LLPacket from Radio.read_ll_packets().
        filename: output path.
    """
    with open(filename, "wb") as f:
        # Section Header Block (type 0x0A0D0D0A)
        # Body: [byte_order_magic:4][major:2][minor:2][section_len:8]
        shb_body = struct.pack("<IHHq", 0x1A2B3C4D, 1, 0, -1)
        f.write(_block(0x0A0D0D0A, shb_body))

        # Interface Description Block (type 0x00000001)
        # Body: [linktype:2][reserved:2][snaplen:4]
        idb_body = struct.pack("<HHI", LINKTYPE_BLUETOOTH_LE_LL, 0, 65535)
        f.write(_block(0x00000001, idb_body))

        # Enhanced Packet Block per packet (type 0x00000006)
        # Body: [interface_id:4][ts_high:4][ts_low:4][cap_len:4][orig_len:4][data:N]
        for pkt in packets:
            ts_us = int(pkt.timestamp * 1_000_000)
            ts_high = (ts_us >> 32) & 0xFFFFFFFF
            ts_low = ts_us & 0xFFFFFFFF
            data = pkt.payload
            epb_body = struct.pack(
                "<IIIII", 0, ts_high, ts_low, len(data), len(data)
            ) + data
            f.write(_block(0x00000006, epb_body))
