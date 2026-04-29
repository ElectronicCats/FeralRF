"""Unit tests for BLE active scanner support (F12).

The firmware reserves PDU type 0x04 for SCAN_RSP and the LL classifier
already tags it; these tests cover the Python-side parser, dataclass,
and merge logic that consume those packets.
"""

from feralrf._ble_scan import BleScanResult, extract_pdu_header, parse_ad_structures


def test_blescanresult_minimal_fields():
    r = BleScanResult(mac="DE:AD:BE:EF:CA:FE", addr_type="public")
    assert r.mac == "DE:AD:BE:EF:CA:FE"
    assert r.addr_type == "public"
    assert r.name is None
    assert r.adv_count == 0
    assert r.scan_rsp_count == 0
    assert r.uuids_16bit == []
    assert r.uuids_128bit == []
    assert r.manufacturer_data == {}
    assert r.raw_advs == []
    assert r.raw_scan_rsps == []


def test_parse_ad_empty_payload_returns_empty_dict():
    assert parse_ad_structures(b"") == {}


def test_parse_ad_flags():
    # AD: len=2, type=0x01 (Flags), value=0x06
    payload = bytes([0x02, 0x01, 0x06])
    out = parse_ad_structures(payload)
    assert out == {"flags": 0x06}


def test_parse_ad_tx_power_signed():
    # AD: len=2, type=0x0A (TX Power), value=-12 (0xF4 as signed int8)
    payload = bytes([0x02, 0x0A, 0xF4])
    out = parse_ad_structures(payload)
    assert out == {"tx_power": -12}


def test_parse_ad_appearance_uint16_le():
    # AD: len=3, type=0x19 (Appearance), value=0x0040 (Generic Phone)
    payload = bytes([0x03, 0x19, 0x40, 0x00])
    out = parse_ad_structures(payload)
    assert out == {"appearance": 0x0040}


def test_parse_ad_complete_name_utf8():
    name = "Soundcore Boom 2"
    name_bytes = name.encode("utf-8")
    payload = bytes([len(name_bytes) + 1, 0x09]) + name_bytes
    out = parse_ad_structures(payload)
    assert out == {"name": "Soundcore Boom 2"}


def test_parse_ad_shortened_name_used_when_no_complete():
    payload = bytes([0x08, 0x08]) + b"PixelXL"
    out = parse_ad_structures(payload)
    assert out == {"name": "PixelXL"}


def test_parse_ad_complete_name_preferred_over_shortened():
    payload = bytes([0x06, 0x08]) + b"Pixel" + bytes([0x0C, 0x09]) + b"Pixel 7 Pro"
    out = parse_ad_structures(payload)
    assert out == {"name": "Pixel 7 Pro"}


def test_parse_ad_name_invalid_utf8_replaced():
    payload = bytes([0x04, 0x09, 0x41, 0xFF, 0x42])
    out = parse_ad_structures(payload)
    assert "name" in out
    assert out["name"].startswith("A") and out["name"].endswith("B")


def test_parse_ad_uuids_16bit_complete():
    # AD type 0x03: complete 16-bit UUID list. Two UUIDs: 0xFE2C, 0x180A
    # Little-endian on wire.
    payload = bytes([0x05, 0x03, 0x2C, 0xFE, 0x0A, 0x18])
    out = parse_ad_structures(payload)
    assert out == {"uuids_16bit": ["FE2C", "180A"]}


def test_parse_ad_uuids_16bit_incomplete_extends():
    # AD 0x02 incomplete UUID list — same handling, also added.
    payload = bytes([0x03, 0x02, 0x2C, 0xFE])
    out = parse_ad_structures(payload)
    assert out == {"uuids_16bit": ["FE2C"]}


def test_parse_ad_uuids_16bit_combined_complete_and_incomplete():
    # Both types in same payload — both extend the same list, in order.
    payload = bytes([0x03, 0x02, 0x2C, 0xFE]) + bytes(  # incomplete: FE2C
        [0x03, 0x03, 0x0A, 0x18]
    )  # complete:   180A
    out = parse_ad_structures(payload)
    assert out == {"uuids_16bit": ["FE2C", "180A"]}


def test_parse_ad_uuids_128bit_complete_canonical_format():
    # 0x07 complete 128-bit UUID list. UUID 0000180A-0000-1000-8000-00805F9B34FB
    # On wire: 16 bytes little-endian = reversed canonical bytes.
    canonical = "0000180a-0000-1000-8000-00805f9b34fb"
    canonical_hex = canonical.replace("-", "")
    wire_bytes = bytes.fromhex(canonical_hex)[::-1]  # little-endian
    payload = bytes([0x11, 0x07]) + wire_bytes
    out = parse_ad_structures(payload)
    assert out == {"uuids_128bit": [canonical]}


def test_parse_ad_uuids_128bit_incomplete_extends():
    canonical = "12345678-1234-5678-1234-567812345678"
    wire = bytes.fromhex(canonical.replace("-", ""))[::-1]
    payload = bytes([0x11, 0x06]) + wire
    out = parse_ad_structures(payload)
    assert out == {"uuids_128bit": [canonical]}


def test_parse_ad_service_data_uuid16():
    # 0x16: UUID (2B LE) + variable data. UUID FE2C, data 8F95F8.
    payload = bytes([0x06, 0x16, 0x2C, 0xFE, 0x8F, 0x95, 0xF8])
    out = parse_ad_structures(payload)
    assert out == {"services_uuid16_data": {"FE2C": b"\x8F\x95\xF8"}}


def test_parse_ad_service_data_multiple_uuids():
    payload = bytes([0x06, 0x16, 0x2C, 0xFE, 0x01, 0x02, 0x03]) + bytes(
        [0x05, 0x16, 0x0A, 0x18, 0xAA, 0xBB]
    )
    out = parse_ad_structures(payload)
    assert out == {
        "services_uuid16_data": {
            "FE2C": b"\x01\x02\x03",
            "180A": b"\xAA\xBB",
        }
    }


def test_parse_ad_manufacturer_data_apple():
    # Apple company ID 0x004C (LE: 0x4C 0x00). Proximity Pairing data.
    payload = bytes([0x05, 0xFF, 0x4C, 0x00, 0x07, 0x19])
    out = parse_ad_structures(payload)
    assert out == {"manufacturer_data": {0x004C: b"\x07\x19"}}


def test_parse_ad_manufacturer_data_multiple_companies():
    payload = bytes([0x05, 0xFF, 0x4C, 0x00, 0x07, 0x19]) + bytes(  # Apple
        [0x05, 0xFF, 0xF4, 0x2B, 0xAA, 0xBB]
    )  # Anker
    out = parse_ad_structures(payload)
    assert out == {"manufacturer_data": {0x004C: b"\x07\x19", 0x2BF4: b"\xAA\xBB"}}


def test_parse_ad_manufacturer_data_too_short_skipped():
    # AD 0xFF with only 1 byte of value (no full company_id) — skipped.
    payload = bytes([0x02, 0xFF, 0x4C])
    out = parse_ad_structures(payload)
    assert "manufacturer_data" not in out


def test_parse_ad_zero_length_skipped_no_infinite_loop():
    # AD len=0 followed by valid AD — must not infinite-loop.
    payload = bytes([0x00, 0x02, 0x01, 0x06])
    out = parse_ad_structures(payload)
    assert out == {"flags": 0x06}


def test_parse_ad_truncated_length_breaks_cleanly():
    # AD claims len=10 but only 3 bytes follow — break, don't raise.
    payload = bytes([0x0A, 0x09, 0x41, 0x42, 0x43])
    out = parse_ad_structures(payload)
    assert out == {}


def test_parse_ad_unknown_type_skipped_correctly():
    # Unknown AD type 0xAB followed by valid Flags.
    payload = bytes([0x03, 0xAB, 0xFF, 0xFF]) + bytes([0x02, 0x01, 0x06])
    out = parse_ad_structures(payload)
    assert out == {"flags": 0x06}


def test_extract_pdu_header_public_address():
    # PDU header [type|RFU][len|RxAdd|TxAdd] + AdvA (6B LE)
    # TxAdd=0 → public. AdvA = DE AD BE EF CA FE display, wire LE = FE CA EF BE AD DE.
    pkt_data = bytes([0x00, 0x06, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE])
    mac, addr_type = extract_pdu_header(pkt_data)
    assert mac == "DE:AD:BE:EF:CA:FE"
    assert addr_type == "public"


def test_extract_pdu_header_random_static():
    # TxAdd=1 (bit 6 of byte 1 = 0x40) + AdvA byte 5 high bits = 0xC0 → static
    pkt_data = bytes([0x00, 0x46, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0xDE | 0xC0])
    mac, addr_type = extract_pdu_header(pkt_data)
    expected_msb = 0xDE | 0xC0
    assert mac == f"{expected_msb:02X}:AD:BE:EF:CA:FE"
    assert addr_type == "random_static"


def test_extract_pdu_header_random_resolvable():
    # TxAdd=1 + high bits 0b01 (0x40)
    pkt_data = bytes([0x00, 0x46, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0x7E])  # 0x7E high bits = 01
    mac, addr_type = extract_pdu_header(pkt_data)
    assert addr_type == "random_resolvable"


def test_extract_pdu_header_random_non_resolvable():
    # TxAdd=1 + high bits 0b00
    pkt_data = bytes([0x00, 0x46, 0xFE, 0xCA, 0xEF, 0xBE, 0xAD, 0x3E])  # 0x3E high bits = 00
    mac, addr_type = extract_pdu_header(pkt_data)
    assert addr_type == "random_non_resolvable"


def test_extract_pdu_header_too_short_returns_none():
    pkt_data = bytes([0x00, 0x06, 0xFE])  # truncated
    mac, addr_type = extract_pdu_header(pkt_data)
    assert mac is None and addr_type is None
