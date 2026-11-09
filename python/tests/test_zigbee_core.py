"""Tests for Increment 1 safe decoder-core primitives."""

import subprocess
import sys
import textwrap
from dataclasses import FrozenInstanceError
from pathlib import Path

import pytest

from feralrf.enums import PHY
from feralrf.zigbee.core import (
    BoundedReadError,
    ByteReader,
    CaptureContext,
    FieldPresence,
    IssueCode,
    Provenance,
    RawCapture,
    SchemaProfile,
    Span,
)

_INTEGER_READS = (
    ("read_u8", 1),
    ("read_u16_le", 2),
    ("read_u32_le", 4),
    ("read_u64_le", 8),
)


@pytest.mark.parametrize(
    ("method", "raw", "expected"),
    (
        ("read_u8", b"\xab", 0xAB),
        ("read_u16_le", b"\x34\x12", 0x1234),
        ("read_u32_le", b"\x78\x56\x34\x12", 0x12345678),
        ("read_u64_le", b"\xef\xcd\xab\x89\x67\x45\x23\x01", 0x0123456789ABCDEF),
    ),
)
def test_integer_reads_are_little_endian(method, raw, expected):
    reader = ByteReader(raw)

    assert getattr(reader, method)() == expected
    assert reader.position == len(raw)
    assert reader.remaining == 0


@pytest.mark.parametrize(("method", "width"), _INTEGER_READS)
def test_integer_exact_fit_succeeds(method, width):
    reader = ByteReader(bytes([0x11]) * width)

    getattr(reader, method)()

    assert reader.position == width
    assert reader.remaining == 0


@pytest.mark.parametrize(("method", "width"), _INTEGER_READS)
def test_integer_one_byte_short_fails_safely(method, width):
    reader = ByteReader(bytes([0x11]) * (width - 1))

    with pytest.raises(BoundedReadError) as excinfo:
        getattr(reader, method)()

    issue = excinfo.value.issue
    assert issue.code == IssueCode.TRUNCATED
    assert issue.absolute_offset == 0
    assert issue.required == width
    assert issue.available == width - 1
    assert reader.position == 0


@pytest.mark.parametrize(("method", "width"), _INTEGER_READS)
def test_integer_zero_length_buffer_fails_safely(method, width):
    reader = ByteReader(b"")

    with pytest.raises(BoundedReadError) as excinfo:
        getattr(reader, method)()

    issue = excinfo.value.issue
    assert issue.absolute_offset == 0
    assert issue.required == width
    assert issue.available == 0
    assert reader.position == 0


@pytest.mark.parametrize(("method", "width"), _INTEGER_READS)
def test_integer_cursor_never_reads_past_end_for_any_size(method, width):
    for size in range(0, width + 4):
        data = bytes(range(size))
        reader = ByteReader(data)

        if size >= width:
            value = getattr(reader, method)()
            assert value == int.from_bytes(data[:width], "little")
            assert reader.position == width
        else:
            with pytest.raises(BoundedReadError) as excinfo:
                getattr(reader, method)()
            assert excinfo.value.issue.available == size
            assert reader.position == 0


def test_read_at_end_raises_instead_of_returning_empty():
    reader = ByteReader(b"\x01")
    assert reader.read_u8() == 1
    assert reader.remaining == 0

    with pytest.raises(BoundedReadError) as excinfo:
        reader.read_u8()

    issue = excinfo.value.issue
    assert issue.absolute_offset == 1
    assert issue.required == 1
    assert issue.available == 0


def test_read_bytes_past_end_does_not_truncate_or_advance():
    reader = ByteReader(b"\x01\x02")
    reader.skip(1)

    with pytest.raises(BoundedReadError) as excinfo:
        reader.read_bytes(3)

    issue = excinfo.value.issue
    assert issue.absolute_offset == 1
    assert issue.required == 3
    assert issue.available == 1
    assert reader.position == 1


def test_zero_length_reads_are_allowed():
    assert ByteReader(b"").read_bytes(0) == b""
    assert ByteReader(b"abc").read_bytes(0) == b""


def test_negative_read_and_skip_lengths_are_rejected():
    reader = ByteReader(b"abc")

    with pytest.raises(ValueError):
        reader.read_bytes(-1)
    with pytest.raises(ValueError):
        reader.skip(-1)


def test_root_span_and_properties():
    reader = ByteReader(b"\xaa\xbb\xcc")

    assert reader.span == Span(0, 3)
    assert reader.start == 0
    assert reader.end == 3
    assert reader.position == 0
    assert reader.remaining == 3
    assert reader.remaining_bytes() == b"\xaa\xbb\xcc"


def test_valid_subspan_preserves_absolute_offsets_and_advances_parent():
    parent = ByteReader(b"\xaa\xbb\xcc\xdd")
    parent.skip(1)

    child = parent.subspan(2)

    assert child.span == Span(1, 3)
    assert child.start == 1
    assert child.end == 3
    assert child.read_u16_le() == 0xCCBB
    assert child.position == 3
    assert parent.position == 3


def test_nested_subspan_preserves_original_absolute_offsets():
    data = bytes(range(8))
    parent = ByteReader(data)
    parent.skip(2)
    child = parent.subspan(4)
    child.skip(1)

    nested = child.subspan(2)

    assert child.span == Span(2, 6)
    assert nested.span == Span(3, 5)
    assert nested.read_bytes(2) == data[3:5]
    assert nested.position == 5


def test_subspan_bounds_are_relative_to_parent_span():
    parent = ByteReader(b"abcdef", Span(1, 5))

    child = parent.subspan_bounds(1, 3)

    assert child.span == Span(2, 4)
    assert child.read_bytes(2) == b"cd"
    assert parent.position == 1


@pytest.mark.parametrize(("start", "end"), ((-1, 2), (2, 1)))
def test_invalid_span_bounds_are_rejected(start, end):
    with pytest.raises(ValueError):
        Span(start, end)


def test_reader_rejects_span_beyond_data_length():
    with pytest.raises(ValueError):
        ByteReader(b"abc", Span(0, 4))


def test_subspan_exceeding_parent_bounds_fails_safely():
    reader = ByteReader(b"abc")

    with pytest.raises(BoundedReadError) as excinfo:
        reader.subspan(4)

    issue = excinfo.value.issue
    assert issue.absolute_offset == 0
    assert issue.required == 4
    assert issue.available == 3
    assert reader.position == 0


def test_subspan_bounds_exceeding_parent_span_are_rejected():
    reader = ByteReader(b"abcdef", Span(1, 5))

    with pytest.raises(ValueError):
        reader.subspan_bounds(0, 5)


def test_nested_truncation_reports_original_absolute_offset():
    reader = ByteReader(bytes(range(10)))
    reader.skip(3)
    child = reader.subspan(4)
    child.skip(3)

    with pytest.raises(BoundedReadError) as excinfo:
        child.read_u16_le()

    issue = excinfo.value.issue
    assert issue.absolute_offset == 6
    assert issue.required == 2
    assert issue.available == 1


def test_truncation_issue_has_exact_required_and_available_metadata():
    reader = ByteReader(b"\x01\x02\x03")

    with pytest.raises(BoundedReadError) as excinfo:
        reader.read_u32_le()

    issue = excinfo.value.issue
    assert issue.code == IssueCode.TRUNCATED
    assert issue.absolute_offset == 0
    assert issue.required == 4
    assert issue.available == 3
    assert issue.layer is None
    assert excinfo.value.absolute_offset == 0
    assert excinfo.value.required == 4
    assert excinfo.value.available == 3


def test_raw_capture_preserves_bytes_and_does_not_apply_fcs_or_phr_policy():
    original = b"\x05\xaa\xbb\xcc\xdd"
    context = CaptureContext(
        phy=PHY.IEEE_802_15_4,
        phr=FieldPresence.ABSENT,
        fcs=FieldPresence.PRESENT,
    )

    capture = RawCapture(data=original, context=context)

    assert capture.data == original
    assert capture.data[:1] == b"\x05"
    assert capture.data[-2:] == b"\xcc\xdd"
    assert len(capture.data) == len(original)


def test_raw_capture_copies_caller_owned_mutable_input():
    source = bytearray(b"\x01\x02\x03")
    capture = RawCapture(data=source, context=CaptureContext())

    source[0] = 0xFF

    assert capture.data == b"\x01\x02\x03"
    assert isinstance(capture.data, bytes)


def test_raw_capture_is_immutable():
    capture = RawCapture(data=b"x", context=CaptureContext())

    with pytest.raises(FrozenInstanceError):
        capture.data = b"y"


def test_raw_capture_reader_uses_same_bytes_and_absolute_span():
    capture = RawCapture(data=b"\x01\x02\x03", context=CaptureContext())

    assert capture.reader().read_bytes(3) == b"\x01\x02\x03"
    assert capture.reader(Span(1, 3)).read_bytes(2) == b"\x02\x03"


def test_missing_context_is_reported_explicitly():
    issues = CaptureContext().compatibility_issues(phy=PHY.IEEE_802_15_4)

    assert len(issues) == 1
    assert issues[0].code == IssueCode.MISSING_CONTEXT
    assert issues[0].layer == "context"


def test_conflicting_context_is_reported_as_mismatch():
    context = CaptureContext(
        phy=PHY.IEEE_802_15_4,
        phr=FieldPresence.ABSENT,
        fcs=FieldPresence.PRESENT,
        schema=SchemaProfile(name="current"),
    )

    issues = context.compatibility_issues(
        phy=PHY.BLE_1M,
        phr=FieldPresence.PRESENT,
        fcs=FieldPresence.ABSENT,
        schema=SchemaProfile(name="other"),
    )

    assert len(issues) == 4
    assert {issue.code for issue in issues} == {IssueCode.CONTEXT_MISMATCH}


def test_known_compatible_context_has_no_issues():
    schema = SchemaProfile(name="baseline")
    context = CaptureContext(
        phy=PHY.IEEE_802_15_4,
        phr=FieldPresence.ABSENT,
        fcs=FieldPresence.PRESENT,
        provenance=Provenance.DERIVED_FROM_SESSION,
        schema=schema,
        session_epoch=7,
    )

    assert (
        context.compatibility_issues(
            phy=PHY.IEEE_802_15_4,
            phr=FieldPresence.ABSENT,
            fcs=FieldPresence.PRESENT,
            schema=schema,
            session_epoch=7,
        )
        == ()
    )


def test_unknown_fcs_and_phr_are_missing_context_not_mismatch():
    context = CaptureContext(phy=PHY.IEEE_802_15_4)

    assert context.phr is FieldPresence.UNKNOWN
    assert context.fcs is FieldPresence.UNKNOWN

    phr_issues = context.compatibility_issues(phr=FieldPresence.ABSENT)
    fcs_issues = context.compatibility_issues(fcs=FieldPresence.PRESENT)

    assert phr_issues[0].code == IssueCode.MISSING_CONTEXT
    assert fcs_issues[0].code == IssueCode.MISSING_CONTEXT


def test_phy_is_not_inferred_from_any_channel_like_value():
    context = CaptureContext(session_epoch=11)

    assert context.phy is None
    assert context.compatibility_issues(phy=PHY.IEEE_802_15_4)[0].code == (
        IssueCode.MISSING_CONTEXT
    )


def test_schema_profile_is_immutable_and_keeps_plain_identity_only():
    profile = SchemaProfile(
        name="baseline",
        ieee_802_15_4_revision="2015",
        zigbee_core_revision="R22",
        zcl_revision="7",
        application_profiles=[0x1234],
    )

    assert profile.application_profiles == (0x1234,)
    assert isinstance(profile.application_profiles, tuple)
    assert SchemaProfile(name="baseline").application_profiles == ()

    with pytest.raises(FrozenInstanceError):
        profile.name = "other"


def test_import_has_no_serial_radio_or_killerbee_side_effects():
    python_dir = Path(__file__).resolve().parents[1]
    script = textwrap.dedent(
        """
        import sys
        import serial
        import serial.tools.list_ports as list_ports

        import feralrf
        from feralrf import radio as radio_module


        def _fail(*args, **kwargs):
            raise AssertionError("hardware access during decoder import")


        serial.Serial = _fail
        list_ports.comports = _fail
        radio_module.Radio.__init__ = _fail

        import feralrf.zigbee.core  # noqa: F401

        assert "killerbee" not in sys.modules
        print("ok")
        """
    )

    result = subprocess.run(
        [sys.executable, "-c", script],
        cwd=str(python_dir),
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "ok"
