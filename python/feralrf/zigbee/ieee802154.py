"""Bounded parsing for the approved reduced IEEE 802.15.4 MAC profile.

This module intentionally recognizes exactly one legacy Zigbee MAC layout.
It is internal, opt-in, and does not classify Zigbee or parse any upper layer.
"""

from dataclasses import dataclass
from enum import Enum
from typing import Dict, Optional, Tuple, Union

from feralrf.enums import PHY
from feralrf.zigbee.core import (
    BoundedReadError,
    DecodeIssue,
    FieldPresence,
    IssueCode,
    RawCapture,
    SchemaProfile,
    Span,
)

IEEE_802_15_4_2015_R22_LEGACY_DATA_MINIMAL = SchemaProfile(
    name="IEEE_802_15_4_2015_R22_LEGACY_DATA_MINIMAL",
    ieee_802_15_4_revision="2015",
    zigbee_core_revision="R22",
)


class MacStatus(str, Enum):
    """Outcome of reduced-profile MAC parsing."""

    SUPPORTED = "supported"
    UNSUPPORTED = "unsupported"
    TRUNCATED = "truncated"
    MISSING_CONTEXT = "missing_context"
    CONTEXT_MISMATCH = "context_mismatch"


@dataclass(frozen=True)
class MacResult:
    """Immutable structural result for the single approved MAC layout."""

    status: MacStatus
    raw: RawCapture
    issues: Tuple[DecodeIssue, ...] = ()
    fcf: Optional[int] = None
    frame_type: Optional[int] = None
    frame_version: Optional[int] = None
    security_indicated: Optional[bool] = None
    pan_id_compression: Optional[bool] = None
    sequence_suppression: Optional[bool] = None
    ie_present: Optional[bool] = None
    destination_addressing_mode: Optional[int] = None
    source_addressing_mode: Optional[int] = None
    sequence_number: Optional[int] = None
    destination_pan_id: Optional[int] = None
    destination_address: Optional[int] = None
    source_address: Optional[int] = None
    fcf_span: Optional[Span] = None
    sequence_span: Optional[Span] = None
    destination_pan_span: Optional[Span] = None
    destination_address_span: Optional[Span] = None
    source_address_span: Optional[Span] = None
    header_span: Optional[Span] = None
    payload_span: Optional[Span] = None
    fcs_span: Optional[Span] = None
    remainder_span: Optional[Span] = None


FcfValue = Union[int, bool]


def _issue_with_layer(issue: DecodeIssue, layer: str) -> DecodeIssue:
    return DecodeIssue(
        code=issue.code,
        message=issue.message,
        absolute_offset=issue.absolute_offset,
        required=issue.required,
        available=issue.available,
        layer=layer,
    )


def _context_status(issues: Tuple[DecodeIssue, ...]) -> MacStatus:
    if any(issue.code is IssueCode.MISSING_CONTEXT for issue in issues):
        return MacStatus.MISSING_CONTEXT
    return MacStatus.CONTEXT_MISMATCH


def _fcf_values(fcf: int) -> Dict[str, FcfValue]:
    return {
        "fcf": fcf,
        "frame_type": fcf & 0b111,
        "security_indicated": bool(fcf & (1 << 3)),
        "pan_id_compression": bool(fcf & (1 << 6)),
        "sequence_suppression": bool(fcf & (1 << 8)),
        "ie_present": bool(fcf & (1 << 9)),
        "destination_addressing_mode": (fcf >> 10) & 0b11,
        "frame_version": (fcf >> 12) & 0b11,
        "source_addressing_mode": (fcf >> 14) & 0b11,
    }


def _is_supported_fcf(values: Dict[str, FcfValue]) -> bool:
    return (
        values["frame_type"] == 0b001
        and values["frame_version"] == 0
        and values["destination_addressing_mode"] == 0b10
        and values["source_addressing_mode"] == 0b10
        and values["pan_id_compression"] is True
        and values["sequence_suppression"] is False
        and values["ie_present"] is False
        and values["security_indicated"] is False
    )


def parse_mac(capture: RawCapture, profile: SchemaProfile) -> MacResult:
    """Parse only the approved reduced profile.

    Unsupported FCF encodings stop after the bounded two-octet FCF. FCS bytes
    are reserved only when the frozen capture context explicitly says they are
    present; their value is never calculated or verified.
    """

    if profile != IEEE_802_15_4_2015_R22_LEGACY_DATA_MINIMAL:
        return MacResult(
            status=MacStatus.UNSUPPORTED,
            raw=capture,
            issues=(
                DecodeIssue(
                    code=IssueCode.UNSUPPORTED,
                    message=f"unsupported MAC schema profile: {profile.name}",
                    layer="mac.profile",
                ),
            ),
        )

    context_issues = capture.context.compatibility_issues(
        phy=PHY.IEEE_802_15_4,
        phr=FieldPresence.ABSENT,
        schema=profile,
    )
    if context_issues:
        return MacResult(
            status=_context_status(context_issues),
            raw=capture,
            issues=context_issues,
        )

    if capture.context.fcs is FieldPresence.UNKNOWN:
        return MacResult(
            status=MacStatus.MISSING_CONTEXT,
            raw=capture,
            issues=(
                DecodeIssue(
                    code=IssueCode.MISSING_CONTEXT,
                    message="explicit FCS presence policy is required",
                    layer="mac.fcs",
                ),
            ),
        )

    data_length = len(capture.data)
    if capture.context.fcs is FieldPresence.PRESENT:
        if data_length < 2:
            return MacResult(
                status=MacStatus.TRUNCATED,
                raw=capture,
                issues=(
                    DecodeIssue(
                        code=IssueCode.TRUNCATED,
                        message="declared FCS requires two octets",
                        absolute_offset=0,
                        required=2,
                        available=data_length,
                        layer="mac.fcs",
                    ),
                ),
            )
        body_end = data_length - 2
        fcs_span = Span(body_end, data_length)
    else:
        body_end = data_length
        fcs_span = None

    reader = capture.reader(Span(0, body_end))
    try:
        fcf = reader.read_u16_le()
    except BoundedReadError as error:
        return MacResult(
            status=MacStatus.TRUNCATED,
            raw=capture,
            issues=(_issue_with_layer(error.issue, "mac.fcf"),),
        )

    values = _fcf_values(fcf)
    fcf_span = Span(0, 2)
    if not _is_supported_fcf(values):
        return MacResult(
            status=MacStatus.UNSUPPORTED,
            raw=capture,
            issues=(
                DecodeIssue(
                    code=IssueCode.UNSUPPORTED,
                    message="FCF is outside the reduced MAC profile",
                    absolute_offset=0,
                    layer="mac.fcf",
                ),
            ),
            fcf_span=fcf_span,
            remainder_span=Span(2, data_length),
            **values,
        )

    try:
        sequence_start = reader.position
        sequence_number = reader.read_u8()
        sequence_span = Span(sequence_start, reader.position)

        destination_pan_start = reader.position
        destination_pan_id = reader.read_u16_le()
        destination_pan_span = Span(destination_pan_start, reader.position)

        destination_address_start = reader.position
        destination_address = reader.read_u16_le()
        destination_address_span = Span(destination_address_start, reader.position)

        source_address_start = reader.position
        source_address = reader.read_u16_le()
        source_address_span = Span(source_address_start, reader.position)
    except BoundedReadError as error:
        return MacResult(
            status=MacStatus.TRUNCATED,
            raw=capture,
            issues=(_issue_with_layer(error.issue, "mac.header"),),
            fcf_span=fcf_span,
            **values,
        )

    header_span = Span(0, reader.position)
    payload_span = Span(reader.position, body_end)
    return MacResult(
        status=MacStatus.SUPPORTED,
        raw=capture,
        fcf_span=fcf_span,
        sequence_number=sequence_number,
        destination_pan_id=destination_pan_id,
        destination_address=destination_address,
        source_address=source_address,
        sequence_span=sequence_span,
        destination_pan_span=destination_pan_span,
        destination_address_span=destination_address_span,
        source_address_span=source_address_span,
        header_span=header_span,
        payload_span=payload_span,
        fcs_span=fcs_span,
        **values,
    )
