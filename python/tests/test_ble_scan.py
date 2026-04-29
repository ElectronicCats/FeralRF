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
