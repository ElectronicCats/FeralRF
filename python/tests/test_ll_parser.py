"""Unit tests for BLE LL / ATT PDU parser (host-side helper)."""
import pytest
from feralrf._ll_parser import parse_ll_pdu, LLPduKind, LL_OPCODE_NAMES


class TestParseLLData:
    def test_empty_data_pdu_llid1(self):
        # LL header: byte0 = LLID=01b (continuation/empty L2CAP), byte1 = length=0
        # Real packet: 01 00 (no payload)
        result = parse_ll_pdu(b"\x01\x00")
        assert result.kind == LLPduKind.DATA_CONT
        assert result.length == 0
        assert result.payload == b""

    def test_l2cap_start_llid2_with_att_write_req(self):
        # LLID=02 (L2CAP start), len=9, L2CAP[len:2 cid:2 att]
        # ATT_WRITE_REQ to handle 0x00d5 with value 01 00:
        # 02 09 05 00 04 00 12 d5 00 01 00
        raw = b"\x02\x09\x05\x00\x04\x00\x12\xd5\x00\x01\x00"
        result = parse_ll_pdu(raw)
        assert result.kind == LLPduKind.DATA_START
        assert result.length == 9
        assert result.payload == raw[2:]

    def test_ll_control_terminate_ind_llid3(self):
        # LLID=03 (LL control), len=2, opcode=0x02 (LL_TERMINATE_IND), reason=0x13
        # 03 02 02 13
        result = parse_ll_pdu(b"\x03\x02\x02\x13")
        assert result.kind == LLPduKind.CONTROL
        assert result.length == 2
        assert result.opcode == 0x02
        assert result.opcode_name == "LL_TERMINATE_IND"
        assert result.payload == b"\x02\x13"

    def test_ll_control_enc_req_llid3(self):
        # LL_ENC_REQ opcode=0x03, payload=22 bytes
        raw = bytes.fromhex("03160300010203040506070800010001020304050607080000")
        # 03=LLID3 16=len(22) 03=opcode then 22 body bytes (1 opcode + 21 fields)
        result = parse_ll_pdu(raw)
        assert result.kind == LLPduKind.CONTROL
        assert result.length == 0x16
        assert result.opcode == 0x03
        assert result.opcode_name == "LL_ENC_REQ"

    def test_unknown_ll_control_opcode(self):
        # LLID=03, len=1, opcode=0xFE (RFU)
        result = parse_ll_pdu(b"\x03\x01\xFE")
        assert result.opcode == 0xFE
        assert result.opcode_name.startswith("LL_RFU")

    def test_truncated_header_returns_none(self):
        assert parse_ll_pdu(b"") is None
        assert parse_ll_pdu(b"\x03") is None  # only header byte 0

    def test_truncated_payload_marked(self):
        # LLID=02, len=10, but only 5 payload bytes provided
        result = parse_ll_pdu(b"\x02\x0a\x01\x02\x03\x04\x05")
        assert result.truncated is True

    def test_llid_zero_is_reserved(self):
        # LLID=00 is reserved; parser should mark it but still return shape
        result = parse_ll_pdu(b"\x00\x00")
        assert result.kind == LLPduKind.RESERVED
