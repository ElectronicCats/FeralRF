"""Unit tests for BLE LL / ATT PDU parser (host-side helper)."""

from feralrf._ll_parser import LLPduKind, parse_ll_pdu


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


class TestParseATT:
    def test_att_write_req(self):
        from feralrf._ll_parser import parse_att_pdu

        # ATT_WRITE_REQ to handle 0x00d5, value=01 00 (CCC notify enable)
        result = parse_att_pdu(b"\x12\xd5\x00\x01\x00")
        assert result.opcode == 0x12
        assert result.opcode_name == "ATT_WRITE_REQ"
        assert result.handle == 0x00D5
        assert result.value == b"\x01\x00"

    def test_att_handle_value_notification(self):
        from feralrf._ll_parser import parse_att_pdu

        # Handle 0x0064, value=AA BB CC
        result = parse_att_pdu(b"\x1b\x64\x00\xaa\xbb\xcc")
        assert result.opcode == 0x1B
        assert result.opcode_name == "ATT_HANDLE_VALUE_NTF"
        assert result.handle == 0x0064
        assert result.value == b"\xaa\xbb\xcc"

    def test_att_error_response(self):
        from feralrf._ll_parser import parse_att_pdu

        # ATT_ERROR_RSP: opcode=01, req_op=0x0a (READ), handle=0x0010, code=0x05 (insuf auth)
        result = parse_att_pdu(b"\x01\x0a\x10\x00\x05")
        assert result.opcode == 0x01
        assert result.opcode_name == "ATT_ERROR_RSP"

    def test_truncated_handle_returns_opcode_only(self):
        from feralrf._ll_parser import parse_att_pdu

        # Just an opcode, no handle bytes
        result = parse_att_pdu(b"\x12")
        assert result.opcode == 0x12
        assert result.handle is None

    def test_extract_att_from_l2cap_start(self):
        from feralrf._ll_parser import parse_att_pdu, parse_ll_pdu

        # LL data PDU containing a complete L2CAP frame with ATT_WRITE_REQ
        raw = b"\x02\x09\x05\x00\x04\x00\x12\xd5\x00\x01\x00"
        ll = parse_ll_pdu(raw)
        # L2CAP header is first 4 bytes of payload: [len:2][cid:2]
        att_bytes = ll.payload[4:]
        att = parse_att_pdu(att_bytes)
        assert att.opcode_name == "ATT_WRITE_REQ"
        assert att.handle == 0x00D5

    def test_att_read_rsp_has_no_handle_field(self):
        """F8f #8 — READ_RSP carries only value bytes, no handle prefix."""
        from feralrf._ll_parser import parse_att_pdu

        # opcode=0x0B (ATT_READ_RSP) + 4 value bytes
        pdu = parse_att_pdu(bytes([0x0B, 0xDE, 0xAD, 0xBE, 0xEF]))
        assert pdu is not None
        assert pdu.opcode == 0x0B
        assert pdu.handle is None, "READ_RSP must not parse value bytes as a handle"
        assert pdu.value is None, (
            "value field is reserved for has_handle PDUs; read responses are not "
            "decoded into the AttPdu.value attribute by parse_att_pdu"
        )

    def test_att_read_blob_rsp_has_no_handle_field(self):
        """F8f #8 — READ_BLOB_RSP carries only value bytes, no handle prefix."""
        from feralrf._ll_parser import parse_att_pdu

        pdu = parse_att_pdu(bytes([0x0D, 0x12, 0x34, 0x56]))
        assert pdu is not None
        assert pdu.opcode == 0x0D
        assert pdu.handle is None
        assert pdu.value is None


class TestExportPcap:
    def test_export_creates_valid_pcapng(self, tmp_path):
        from feralrf._ll_parser import export_pcap
        from feralrf.radio import LLPacket

        pkts = [
            LLPacket(
                direction="M->S",
                channel=10,
                rssi_dbm=-55,
                event_counter=1,
                payload=b"\x02\x09\x05\x00\x04\x00\x12\xd5\x00\x01\x00",
                timestamp=1700000000.0,
            ),
            LLPacket(
                direction="S->M",
                channel=10,
                rssi_dbm=-58,
                event_counter=1,
                payload=b"\x02\x05\x01\x00\x04\x00\x13",
                timestamp=1700000000.001,
            ),
        ]
        path = tmp_path / "f8b.pcapng"
        export_pcap(pkts, str(path))
        data = path.read_bytes()
        # pcap-NG starts with Section Header Block magic 0x0A0D0D0A
        assert data[:4] == b"\x0a\x0d\x0d\x0a"
        # Byte order magic 0x1A2B3C4D should appear in the section header
        assert b"\x4d\x3c\x2b\x1a" in data[:32]
        # Should contain at least 2 enhanced packet blocks (type 0x06)
        assert data.count(b"\x06\x00\x00\x00") >= 2

    def test_export_empty_list_creates_section_header_only(self, tmp_path):
        from feralrf._ll_parser import export_pcap

        path = tmp_path / "empty.pcapng"
        export_pcap([], str(path))
        data = path.read_bytes()
        assert data[:4] == b"\x0a\x0d\x0d\x0a"
        # No EPB
        assert data.count(b"\x06\x00\x00\x00") == 0
