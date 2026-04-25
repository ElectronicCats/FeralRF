"""
CONNECT_IND PDU builder (pure Python reference encoder).

Byte-for-byte reference for the firmware C encoder in
`firmware/cc1352/src/ble_conn_pdu.c`. Both must produce identical bytes
given identical input fields. This module is tested by
`python/tests/test_connect_ind_pdu.py` and used as the oracle when Task 8
validates the firmware's on-wire CONNECT_IND.

Spec: BLE 5.0 Vol 6 Part B §2.3.3.1.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

CONNECT_IND_PDU_TYPE = 0b0101


@dataclass(frozen=True)
class BleConnIndFields:
    init_addr: bytes  # 6 B, LE as transmitted (octet 0 first)
    init_addr_random: bool  # TxAdd
    adv_addr: bytes  # 6 B, LE as transmitted
    adv_addr_random: bool  # RxAdd
    access_addr: int  # 32-bit
    crc_init: int  # 24-bit
    win_size: int  # 1 B, units of 1.25 ms
    win_offset: int  # 2 B, units of 1.25 ms
    interval: int  # 2 B, units of 1.25 ms
    latency: int  # 2 B
    timeout: int  # 2 B, units of 10 ms
    channel_map: bytes  # 5 B
    hop_increment: int  # 5 bits
    sca: int  # 3 bits

    def __post_init__(self) -> None:
        if len(self.init_addr) != 6:
            raise ValueError("init_addr must be 6 B")
        if len(self.adv_addr) != 6:
            raise ValueError("adv_addr must be 6 B")
        if len(self.channel_map) != 5:
            raise ValueError("channel_map must be 5 B")
        if not 0 <= self.hop_increment <= 0x1F:
            raise ValueError("hop_increment must fit in 5 bits")
        if not 0 <= self.sca <= 0x07:
            raise ValueError("sca must fit in 3 bits")


def build_ll_data(f: BleConnIndFields) -> bytes:
    buf = bytearray(22)
    struct.pack_into("<I", buf, 0, f.access_addr)
    buf[4] = f.crc_init & 0xFF
    buf[5] = (f.crc_init >> 8) & 0xFF
    buf[6] = (f.crc_init >> 16) & 0xFF
    buf[7] = f.win_size & 0xFF
    struct.pack_into("<H", buf, 8, f.win_offset)
    struct.pack_into("<H", buf, 10, f.interval)
    struct.pack_into("<H", buf, 12, f.latency)
    struct.pack_into("<H", buf, 14, f.timeout)
    buf[16:21] = f.channel_map
    buf[21] = (f.hop_increment & 0x1F) | ((f.sca & 0x07) << 5)
    return bytes(buf)


def build_connect_ind_pdu(f: BleConnIndFields) -> bytes:
    ll = build_ll_data(f)
    payload = f.init_addr + f.adv_addr + ll  # 6 + 6 + 22 = 34
    hdr0 = CONNECT_IND_PDU_TYPE & 0x0F
    if f.init_addr_random:
        hdr0 |= 1 << 6  # TxAdd
    if f.adv_addr_random:
        hdr0 |= 1 << 7  # RxAdd
    header = bytes([hdr0, len(payload)])
    return header + payload
