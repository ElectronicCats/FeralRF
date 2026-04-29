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
