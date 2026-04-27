"""Contract tests for CMD_DEBUG_TIMING / RSP_DEBUG_TIMING."""

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
    e1 = struct.pack("<HIIHB", 0, 0x10000000, 0x10100000, 0x1402, 0)
    e2 = struct.pack("<HIIHB", 1, 0x10100000, 0x10200000, 0x1400, 1)
    payload = bytes([2]) + e1 + e2
    parsed = DebugTimingResponse.parse(payload)
    assert parsed.count == 2
    assert parsed.entries == [
        DebugTimingEntry(
            event_idx=0, start_rat=0x10000000, end_rat=0x10100000, status=0x1402, num_sent=0
        ),
        DebugTimingEntry(
            event_idx=1, start_rat=0x10100000, end_rat=0x10200000, status=0x1400, num_sent=1
        ),
    ]


def test_response_rejects_truncated_entry():
    payload = bytes([1]) + b"\x00\x00"  # claims 1 entry, truncated
    try:
        DebugTimingResponse.parse(payload)
    except ValueError:
        return
    raise AssertionError("expected ValueError on truncated payload")
