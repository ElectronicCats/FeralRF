"""Contract tests for CMD_DEBUG_TIMING / RSP_DEBUG_TIMING.

Wire layout per entry (Session 4 extension, was 13 B in Session 3):
    event_idx    u16 LE
    start_rat    u32 LE
    end_rat      u32 LE
    status       u16 LE
    num_sent     u8     -- pOutput.nTxEntryDone
    n_tx         u8     -- pOutput.nTx (total TX incl. auto-empty + retrans)
    n_rx_ok      u8
    n_rx_nok     u8
    n_rx_ignored u8
    pkt_status   u8     -- packed bitfield (see DebugTimingEntry properties)
  = 18 bytes per entry; frame = count(u8) + count*18 bytes.
"""

import struct

from feralrf._responses import DebugTimingEntry, DebugTimingResponse
from feralrf.commands import CommandBuilder
from feralrf.enums import Command, Response


def test_command_id_and_response_id_are_in_enum():
    assert Command.DEBUG_TIMING == 0x47
    assert Response.DEBUG_TIMING == 0xA8


def test_command_builder_has_no_payload():
    assert CommandBuilder.debug_timing() == b""


def test_response_parses_zero_entries():
    payload = bytes([0])  # count=0, no entries
    parsed = DebugTimingResponse.parse(payload)
    assert parsed.count == 0
    assert parsed.entries == []


def test_response_parses_two_entries():
    e1 = struct.pack("<HIIHBBBBBB", 0, 0x10000000, 0x10100000, 0x1402, 0, 0, 0, 0, 0, 0x00)
    e2 = struct.pack("<HIIHBBBBBB", 1, 0x10100000, 0x10200000, 0x1400, 1, 2, 1, 0, 0, 0x41)
    payload = bytes([2]) + e1 + e2
    parsed = DebugTimingResponse.parse(payload)
    assert parsed.count == 2
    assert parsed.entries == [
        DebugTimingEntry(
            event_idx=0,
            start_rat=0x10000000,
            end_rat=0x10100000,
            status=0x1402,
            num_sent=0,
            n_tx=0,
            n_rx_ok=0,
            n_rx_nok=0,
            n_rx_ignored=0,
            pkt_status=0x00,
        ),
        DebugTimingEntry(
            event_idx=1,
            start_rat=0x10100000,
            end_rat=0x10200000,
            status=0x1400,
            num_sent=1,
            n_tx=2,
            n_rx_ok=1,
            n_rx_nok=0,
            n_rx_ignored=0,
            pkt_status=0x41,
        ),
    ]


def test_response_rejects_truncated_entry():
    payload = bytes([1]) + b"\x00\x00"  # claims 1 entry, truncated
    try:
        DebugTimingResponse.parse(payload)
    except ValueError:
        return
    raise AssertionError("expected ValueError on truncated payload")


def test_debug_timing_parses_extended_entry():
    """One full 18-byte entry: NOSYNC with all new fields zero."""
    payload = bytes(
        [
            0x01,  # count
            0x05,
            0x00,  # event_idx = 5
            0xAD,
            0xDE,
            0x00,
            0x00,  # start_rat = 0xDEAD
            0xEF,
            0xBE,
            0x00,
            0x00,  # end_rat   = 0xBEEF
            0x02,
            0x14,  # status    = 0x1402 (NOSYNC)
            0x00,  # num_sent
            0x00,  # n_tx
            0x00,  # n_rx_ok
            0x00,  # n_rx_nok
            0x00,  # n_rx_ignored
            0x00,  # pkt_status
        ]
    )
    rsp = DebugTimingResponse.parse(payload)
    assert len(rsp.entries) == 1
    e = rsp.entries[0]
    assert e.event_idx == 5
    assert e.start_rat == 0xDEAD
    assert e.end_rat == 0xBEEF
    assert e.status == 0x1402
    assert e.num_sent == 0
    assert e.n_tx == 0
    assert e.n_rx_ok == 0
    assert e.n_rx_nok == 0
    assert e.n_rx_ignored == 0
    assert e.pkt_status == 0x00


def test_debug_timing_pkt_status_bits():
    """pkt_status bit layout (matches firmware packing in radio_if.c):
    bit0 = bTimeStampValid
    bit1 = bLastCrcErr
    bit2 = bLastIgnored
    bit3 = bLastEmpty
    bit4 = bLastCtrl
    bit5 = bLastMd
    bit6 = bLastAck
    """
    e = DebugTimingEntry(
        event_idx=0,
        start_rat=0,
        end_rat=0,
        status=0,
        num_sent=0,
        n_tx=0,
        n_rx_ok=0,
        n_rx_nok=0,
        n_rx_ignored=0,
        pkt_status=0b0100_0001,  # bTimeStampValid + bLastAck
    )
    assert e.b_time_stamp_valid is True
    assert e.b_last_crc_err is False
    assert e.b_last_ignored is False
    assert e.b_last_empty is False
    assert e.b_last_ctrl is False
    assert e.b_last_md is False
    assert e.b_last_ack is True
