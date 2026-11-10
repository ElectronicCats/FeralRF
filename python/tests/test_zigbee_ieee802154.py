"""Tests for the reduced IEEE 802.15.4 MAC parser profile."""

import subprocess
import sys
import textwrap
from pathlib import Path
from typing import Optional

import pytest

from feralrf.enums import PHY
from feralrf.zigbee.core import (
    CaptureContext,
    FieldPresence,
    IssueCode,
    RawCapture,
    SchemaProfile,
    Span,
)
from feralrf.zigbee.ieee802154 import (
    IEEE_802_15_4_2015_R22_LEGACY_DATA_MINIMAL,
    MacStatus,
    parse_mac,
)

SUPPORTED_PROFILE = IEEE_802_15_4_2015_R22_LEGACY_DATA_MINIMAL
HEADER = bytes.fromhex("41 88 5a 34 12 78 56 bc 9a")
PAYLOAD = bytes.fromhex("de ad")
FCS = bytes.fromhex("00 00")
FRAME_WITH_FCS = HEADER + PAYLOAD + FCS


def _capture(
    data: bytes,
    *,
    fcs: FieldPresence = FieldPresence.PRESENT,
    crc_ok: bool = True,
    phy: Optional[PHY] = PHY.IEEE_802_15_4,
    phr: FieldPresence = FieldPresence.ABSENT,
    schema: Optional[SchemaProfile] = SUPPORTED_PROFILE,
) -> RawCapture:
    return RawCapture(
        data=data,
        context=CaptureContext(phy=phy, phr=phr, fcs=fcs, schema=schema),
        crc_ok=crc_ok,
    )


def test_supported_canonical_frame_has_exact_values_and_absolute_spans():
    capture = _capture(FRAME_WITH_FCS)

    result = parse_mac(capture, SUPPORTED_PROFILE)

    assert result.status is MacStatus.SUPPORTED
    assert result.raw is capture
    assert result.raw.data == FRAME_WITH_FCS
    assert result.fcf == 0x8841
    assert result.frame_type == 0b001
    assert result.frame_version == 0
    assert result.pan_id_compression is True
    assert result.sequence_suppression is False
    assert result.ie_present is False
    assert result.destination_addressing_mode == 0b10
    assert result.source_addressing_mode == 0b10
    assert result.sequence_number == 0x5A
    assert result.destination_pan_id == 0x1234
    assert result.destination_address == 0x5678
    assert result.source_address == 0x9ABC
    assert result.security_indicated is False
    assert result.fcf_span == Span(0, 2)
    assert result.sequence_span == Span(2, 3)
    assert result.destination_pan_span == Span(3, 5)
    assert result.destination_address_span == Span(5, 7)
    assert result.source_address_span == Span(7, 9)
    assert result.header_span == Span(0, 9)
    assert result.payload_span == Span(9, 11)
    assert result.fcs_span == Span(11, 13)
    assert result.remainder_span is None
    assert result.issues == ()


def test_fcs_present_reserves_exactly_final_two_opaque_bytes():
    result = parse_mac(_capture(FRAME_WITH_FCS), SUPPORTED_PROFILE)

    assert result.raw.data[result.payload_span.start : result.payload_span.end] == PAYLOAD
    assert result.raw.data[result.fcs_span.start : result.fcs_span.end] == FCS


def test_fcs_absent_leaves_all_post_header_bytes_in_payload():
    raw = HEADER + PAYLOAD

    result = parse_mac(
        _capture(raw, fcs=FieldPresence.ABSENT),
        SUPPORTED_PROFILE,
    )

    assert result.status is MacStatus.SUPPORTED
    assert result.payload_span == Span(9, 11)
    assert result.fcs_span is None
    assert result.raw.data[result.payload_span.start : result.payload_span.end] == PAYLOAD


def test_fcs_unknown_reports_missing_context_without_payload_guess():
    result = parse_mac(
        _capture(FRAME_WITH_FCS, fcs=FieldPresence.UNKNOWN),
        SUPPORTED_PROFILE,
    )

    assert result.status is MacStatus.MISSING_CONTEXT
    assert result.payload_span is None
    assert result.fcs_span is None
    assert result.raw.data == FRAME_WITH_FCS
    assert [issue.code for issue in result.issues] == [IssueCode.MISSING_CONTEXT]


@pytest.mark.parametrize("fcs", (FieldPresence.PRESENT, FieldPresence.ABSENT))
def test_supported_header_may_have_empty_payload(fcs):
    raw = HEADER + (FCS if fcs is FieldPresence.PRESENT else b"")

    result = parse_mac(_capture(raw, fcs=fcs), SUPPORTED_PROFILE)

    assert result.status is MacStatus.SUPPORTED
    assert result.header_span == Span(0, 9)
    assert result.payload_span == Span(9, 9)
    assert result.fcs_span == (Span(9, 11) if fcs is FieldPresence.PRESENT else None)


@pytest.mark.parametrize(
    ("length", "absolute_offset", "required", "available"),
    (
        (0, 0, 2, 0),
        (1, 0, 2, 1),
        (2, 2, 1, 0),
        (3, 3, 2, 0),
        (4, 3, 2, 1),
        (5, 5, 2, 0),
        (6, 5, 2, 1),
        (7, 7, 2, 0),
        (8, 7, 2, 1),
    ),
)
def test_each_supported_header_boundary_is_bounded(length, absolute_offset, required, available):
    raw = HEADER[:length]

    result = parse_mac(
        _capture(raw, fcs=FieldPresence.ABSENT),
        SUPPORTED_PROFILE,
    )

    assert result.status is MacStatus.TRUNCATED
    assert result.raw.data == raw
    assert len(result.issues) == 1
    issue = result.issues[0]
    assert issue.code is IssueCode.TRUNCATED
    assert issue.absolute_offset == absolute_offset
    assert issue.required == required
    assert issue.available == available


def test_declared_fcs_present_requires_two_total_bytes_before_parsing():
    result = parse_mac(_capture(b"\x41"), SUPPORTED_PROFILE)

    assert result.status is MacStatus.TRUNCATED
    assert result.issues[0].absolute_offset == 0
    assert result.issues[0].required == 2
    assert result.issues[0].available == 1
    assert result.issues[0].layer == "mac.fcs"


@pytest.mark.parametrize(
    "fcf",
    (
        0x8840,
        0x9841,
        0x8C41,
        0xC841,
        0x8801,
        0x8941,
        0x8A41,
        0x8849,
    ),
)
def test_out_of_profile_fcf_stops_unsupported_before_conditional_fields(fcf):
    raw = fcf.to_bytes(2, "little") + bytes(range(2, 14))
    capture = _capture(raw, fcs=FieldPresence.ABSENT)

    result = parse_mac(capture, SUPPORTED_PROFILE)

    assert result.status is MacStatus.UNSUPPORTED
    assert result.raw is capture
    assert result.raw.data == raw
    assert result.fcf == fcf
    assert result.fcf_span == Span(0, 2)
    assert result.sequence_span is None
    assert result.destination_pan_span is None
    assert result.destination_address_span is None
    assert result.source_address_span is None
    assert result.header_span is None
    assert result.payload_span is None
    assert result.fcs_span is None
    assert result.remainder_span == Span(2, len(raw))
    assert [issue.code for issue in result.issues] == [IssueCode.UNSUPPORTED]


def test_crc_false_is_preserved_without_fcs_validation():
    capture = _capture(FRAME_WITH_FCS, crc_ok=False)

    result = parse_mac(capture, SUPPORTED_PROFILE)

    assert result.status is MacStatus.SUPPORTED
    assert result.raw is capture
    assert result.raw.crc_ok is False
    assert result.raw.data == FRAME_WITH_FCS


@pytest.mark.parametrize(
    ("context_overrides", "status", "code"),
    (
        ({"phy": None}, MacStatus.MISSING_CONTEXT, IssueCode.MISSING_CONTEXT),
        ({"schema": None}, MacStatus.MISSING_CONTEXT, IssueCode.MISSING_CONTEXT),
        (
            {"phy": PHY.BLE_1M},
            MacStatus.CONTEXT_MISMATCH,
            IssueCode.CONTEXT_MISMATCH,
        ),
        (
            {"phr": FieldPresence.PRESENT},
            MacStatus.CONTEXT_MISMATCH,
            IssueCode.CONTEXT_MISMATCH,
        ),
    ),
)
def test_required_capture_context_is_explicit(context_overrides, status, code):
    result = parse_mac(
        _capture(FRAME_WITH_FCS, **context_overrides),
        SUPPORTED_PROFILE,
    )

    assert result.status is status
    assert result.payload_span is None
    assert code in {issue.code for issue in result.issues}
    assert result.raw.data == FRAME_WITH_FCS


def test_other_schema_profile_is_unsupported_without_parsing_layout():
    other = SchemaProfile(
        name="other",
        ieee_802_15_4_revision="2015",
        zigbee_core_revision="R22",
    )

    result = parse_mac(_capture(FRAME_WITH_FCS, schema=other), other)

    assert result.status is MacStatus.UNSUPPORTED
    assert result.fcf is None
    assert result.payload_span is None
    assert result.raw.data == FRAME_WITH_FCS


def test_every_truncated_canonical_prefix_returns_without_crashing():
    allowed = {
        MacStatus.SUPPORTED,
        MacStatus.TRUNCATED,
        MacStatus.UNSUPPORTED,
        MacStatus.MISSING_CONTEXT,
        MacStatus.CONTEXT_MISMATCH,
    }

    for length in range(len(FRAME_WITH_FCS)):
        raw = FRAME_WITH_FCS[:length]
        result = parse_mac(_capture(raw), SUPPORTED_PROFILE)

        assert result.status in allowed
        assert result.raw.data == raw


def test_import_parser_has_no_hardware_side_effects():
    python_dir = Path(__file__).resolve().parents[1]
    script = textwrap.dedent("""
        import sys
        import serial
        import serial.tools.list_ports as list_ports

        import feralrf
        from feralrf import radio as radio_module


        def _fail(*args, **kwargs):
            raise AssertionError("hardware access during MAC parser import")


        serial.Serial = _fail
        list_ports.comports = _fail
        radio_module.Radio.__init__ = _fail

        import feralrf.zigbee.ieee802154  # noqa: F401

        assert "killerbee" not in sys.modules
        print("ok")
        """)

    result = subprocess.run(
        [sys.executable, "-c", script],
        cwd=str(python_dir),
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "ok"
