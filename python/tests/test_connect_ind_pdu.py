"""
FeralRF - CONNECT_IND PDU byte-layout contract test.

Spec: BLE 5.0 Vol 6 Part B §2.3.3.1.
    Header (2 B): LLID/PDU-type (1 B) + Length (1 B)
        Byte 0 bits [3:0] = 0b0101 (CONNECT_IND), bit 6 = TxAdd (InitA type),
                 bit 7 = RxAdd (AdvA type).
    Payload (34 B): InitA (6 B) || AdvA (6 B) || LLData (22 B)
    LLData layout:
        AA (4) || CRCInit (3) || WinSize (1) || WinOffset (2) ||
        Interval (2) || Latency (2) || Timeout (2) || ChM (5) || Hop|SCA (1)
"""

import struct

from feralrf.ble.connect_ind import BleConnIndFields, build_connect_ind_pdu, build_ll_data


def test_ll_data_layout_matches_spec():
    fields = BleConnIndFields(
        init_addr=b"\x01\xee\xdd\xcc\xbb\xaa",
        init_addr_random=True,
        adv_addr=b"\x09\xe1\x8d\x62\x32\xdc",
        adv_addr_random=False,
        access_addr=0xAF9A5C3E,
        crc_init=0x123456,
        win_size=3,
        win_offset=7,
        interval=24,  # 30 ms / 1.25 ms = 24
        latency=0,
        timeout=100,
        channel_map=b"\xff\xff\xff\xff\x1f",
        hop_increment=11,
        sca=0,
    )

    ll = build_ll_data(fields)

    assert len(ll) == 22
    assert ll[0:4] == struct.pack("<I", 0xAF9A5C3E)
    assert ll[4:7] == b"\x56\x34\x12"
    assert ll[7] == 3
    assert ll[8:10] == struct.pack("<H", 7)
    assert ll[10:12] == struct.pack("<H", 24)
    assert ll[12:14] == struct.pack("<H", 0)
    assert ll[14:16] == struct.pack("<H", 100)
    assert ll[16:21] == b"\xff\xff\xff\xff\x1f"
    assert ll[21] == 11  # SCA=0, hop in low 5 bits


def test_connect_ind_pdu_header_bits():
    fields = BleConnIndFields(
        init_addr=b"\x01\xee\xdd\xcc\xbb\xaa",
        init_addr_random=True,  # TxAdd = 1
        adv_addr=b"\x09\xe1\x8d\x62\x32\xdc",
        adv_addr_random=False,  # RxAdd = 0
        access_addr=0xAF9A5C3E,
        crc_init=0,
        win_size=3,
        win_offset=0,
        interval=24,
        latency=0,
        timeout=100,
        channel_map=b"\xff\xff\xff\xff\x1f",
        hop_increment=11,
        sca=0,
    )

    pdu = build_connect_ind_pdu(fields)

    # Header byte 0: PDU type in bits [3:0] = 0b0101, TxAdd bit 6 = 1, RxAdd bit 7 = 0
    assert (pdu[0] & 0x0F) == 0b0101
    assert (pdu[0] >> 6) & 0x01 == 1  # TxAdd
    assert (pdu[0] >> 7) & 0x01 == 0  # RxAdd
    # Header byte 1: payload length = 34 (6 InitA + 6 AdvA + 22 LLData)
    assert pdu[1] == 34
    assert len(pdu) == 36


def test_connect_ind_pdu_payload_order():
    fields = BleConnIndFields(
        init_addr=b"\x01\xee\xdd\xcc\xbb\xaa",
        init_addr_random=True,
        adv_addr=b"\x09\xe1\x8d\x62\x32\xdc",
        adv_addr_random=False,
        access_addr=0xAF9A5C3E,
        crc_init=0x123456,
        win_size=3,
        win_offset=7,
        interval=24,
        latency=0,
        timeout=100,
        channel_map=b"\xff\xff\xff\xff\x1f",
        hop_increment=11,
        sca=0,
    )

    pdu = build_connect_ind_pdu(fields)

    # Bytes 2..7 = InitA (little-endian MAC as transmitted — octet 0 first)
    assert pdu[2:8] == b"\x01\xee\xdd\xcc\xbb\xaa"
    # Bytes 8..13 = AdvA
    assert pdu[8:14] == b"\x09\xe1\x8d\x62\x32\xdc"
    # Bytes 14..35 = 22 B LLData, matches build_ll_data()
    assert pdu[14:36] == build_ll_data(fields)
