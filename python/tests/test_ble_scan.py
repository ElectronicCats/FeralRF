"""Unit tests for BLE active scanner support (F12).

The firmware reserves PDU type 0x04 for SCAN_RSP and the LL classifier
already tags it; these tests cover the Python-side parser, dataclass,
and merge logic that consume those packets.
"""

from feralrf._ble_scan import BleScanResult, parse_ad_structures


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
