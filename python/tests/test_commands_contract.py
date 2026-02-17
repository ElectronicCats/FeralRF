"""
FeralRF - Command payload contract tests
"""

import struct

from feralrf.commands import CommandBuilder
from feralrf.enums import Command, PHY
from feralrf.protocol import build_frame, cobs_decode, parse_frame
from feralrf.radio import Radio


def test_command_builder_returns_payload_only():
    assert CommandBuilder.radio_init() == b""
    assert CommandBuilder.get_info() == b""
    assert CommandBuilder.rx_start() == b""
    assert CommandBuilder.rx_stop() == b""
    assert CommandBuilder.jam_stop() == b""
    assert CommandBuilder.spectrum_stop() == b""

    assert CommandBuilder.set_channel(37) == b"\x25"
    assert CommandBuilder.set_power(-1) == b"\xff"
    assert CommandBuilder.rx_set_promiscuous(True) == b"\x01"
    assert CommandBuilder.rx_set_promiscuous(False) == b"\x00"

    assert CommandBuilder.set_phy(PHY.BLE_1M, 37, 2402000000) == struct.pack(
        "<BHI", PHY.BLE_1M, 37, 2402000000
    )
    assert CommandBuilder.tx_raw(b"\xAA\xBB", power_dbm=-10) == b"\x02\xAA\xBB\xF6"
    assert CommandBuilder.jam_continuous(20, 5) == b"\x14\x05"
    assert CommandBuilder.spectrum_scan(2400000000, 2480000000, 1000, 10, 10) == struct.pack(
        "<IIHBB", 2400000000, 2480000000, 1000, 10, 10
    )


def test_command_id_is_only_in_frame_header():
    frame = build_frame(Command.SET_CHANNEL, 0x01, CommandBuilder.set_channel(37))
    raw = cobs_decode(frame)
    cmd_id, seq, payload = parse_frame(raw)

    assert cmd_id == Command.SET_CHANNEL
    assert seq == 0x01
    assert payload == b"\x25"


def test_radio_default_baudrate_is_921600():
    radio = Radio()
    assert radio.baudrate == 921600
