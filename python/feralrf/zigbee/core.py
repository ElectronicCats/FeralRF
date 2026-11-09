"""Safe decoder-core primitives for host-side Zigbee work.

This module deliberately stops short of protocol interpretation. It provides
immutable capture context, raw byte ownership, structured issues, and bounded
readers that preserve absolute offsets into the original buffer.
"""

from dataclasses import dataclass
from enum import Enum
from typing import List, Optional, Tuple

from feralrf.enums import PHY


class FieldPresence(str, Enum):
    """Tri-state presence of an on-wire field."""

    PRESENT = "present"
    ABSENT = "absent"
    UNKNOWN = "unknown"


class Provenance(str, Enum):
    """How a capture-context value was established."""

    UNKNOWN = "unknown"
    DECLARED_BY_CALLER = "declared_by_caller"
    DERIVED_FROM_SESSION = "derived_from_session"
    VERIFIED_FROM_SOURCE = "verified_from_source"


class IssueCode(str, Enum):
    """Structured decoder issue codes used by this increment."""

    MISSING_CONTEXT = "missing_context"
    CONTEXT_MISMATCH = "context_mismatch"
    TRUNCATED = "truncated"
    UNSUPPORTED = "unsupported"


@dataclass(frozen=True)
class SchemaProfile:
    """Identity for the decoder schema/profile a later parser expects.

    ``application_profiles`` is only an identity allowlist. This increment does
    not assign any semantics to profile IDs.
    """

    name: str
    ieee_802_15_4_revision: Optional[str] = None
    zigbee_core_revision: Optional[str] = None
    zcl_revision: Optional[str] = None
    application_profiles: Tuple[int, ...] = ()

    def __post_init__(self) -> None:
        object.__setattr__(self, "application_profiles", tuple(self.application_profiles))


@dataclass(frozen=True)
class DecodeIssue:
    """A structured, non-exceptional decoder result.

    ``absolute_offset`` is relative to the start of the original raw buffer.
    Context issues may omit byte-specific fields.
    """

    code: IssueCode
    message: str = ""
    absolute_offset: Optional[int] = None
    required: Optional[int] = None
    available: Optional[int] = None
    layer: Optional[str] = None


class BoundedReadError(Exception):
    """Deliberate error type for an attempt to read outside a bounded span."""

    def __init__(self, issue: DecodeIssue):
        self.issue = issue
        super().__init__(
            f"{issue.code.value} at absolute offset {issue.absolute_offset}: "
            f"required {issue.required}, available {issue.available}"
        )

    @property
    def absolute_offset(self) -> Optional[int]:
        return self.issue.absolute_offset

    @property
    def required(self) -> Optional[int]:
        return self.issue.required

    @property
    def available(self) -> Optional[int]:
        return self.issue.available


@dataclass(frozen=True)
class CaptureContext:
    """Provenance and byte-policy context for a raw capture.

    Missing values are represented explicitly rather than inferred. In
    particular, no method derives PHY from a channel.
    """

    phy: Optional[PHY] = None
    phr: FieldPresence = FieldPresence.UNKNOWN
    fcs: FieldPresence = FieldPresence.UNKNOWN
    provenance: Provenance = Provenance.UNKNOWN
    schema: Optional[SchemaProfile] = None
    session_epoch: Optional[int] = None

    def compatibility_issues(
        self,
        *,
        phy: Optional[PHY] = None,
        phr: Optional[FieldPresence] = None,
        fcs: Optional[FieldPresence] = None,
        schema: Optional[SchemaProfile] = None,
        session_epoch: Optional[int] = None,
    ) -> Tuple[DecodeIssue, ...]:
        """Return issues for fields that are required by a later layer.

        ``None`` means "not required by this check". Unknown PHR/FCS policy or
        a missing schema is reported as ``missing_context``; a known but
        different value is reported as ``context_mismatch``.
        """

        checks = (
            ("phy", phy, self.phy, None),
            ("phr", phr, self.phr, FieldPresence.UNKNOWN),
            ("fcs", fcs, self.fcs, FieldPresence.UNKNOWN),
            ("schema", schema, self.schema, None),
            ("session_epoch", session_epoch, self.session_epoch, None),
        )

        issues: List[DecodeIssue] = []
        for field_name, required, actual, unknown_value in checks:
            if required is None:
                continue
            if actual is None or actual == unknown_value:
                issues.append(
                    DecodeIssue(
                        code=IssueCode.MISSING_CONTEXT,
                        message=f"{field_name} is required but missing or unknown",
                        layer="context",
                    )
                )
            elif actual != required:
                issues.append(
                    DecodeIssue(
                        code=IssueCode.CONTEXT_MISMATCH,
                        message=(
                            f"{field_name} mismatch: required {required!r}, "
                            f"got {actual!r}"
                        ),
                        layer="context",
                    )
                )
        return tuple(issues)


@dataclass(frozen=True)
class Span:
    """Absolute half-open byte range ``[start, end)`` in a raw buffer."""

    start: int
    end: int

    def __post_init__(self) -> None:
        if self.start < 0:
            raise ValueError("span start must be non-negative")
        if self.end < self.start:
            raise ValueError("span end must not precede span start")

    @property
    def length(self) -> int:
        return self.end - self.start


class ByteReader:
    """Bounded reader over immutable bytes with absolute offsets."""

    __slots__ = ("_data", "_span", "_position")

    def __init__(self, data: bytes, span: Optional[Span] = None):
        self._data = bytes(data)
        if span is None:
            span = Span(0, len(self._data))
        if span.end > len(self._data):
            raise ValueError("span end exceeds raw data length")
        self._span = span
        self._position = span.start

    @property
    def data(self) -> bytes:
        return self._data

    @property
    def span(self) -> Span:
        return self._span

    @property
    def start(self) -> int:
        return self._span.start

    @property
    def end(self) -> int:
        return self._span.end

    @property
    def position(self) -> int:
        return self._position

    @property
    def remaining(self) -> int:
        return self._span.end - self._position

    def read_u8(self) -> int:
        return int.from_bytes(self._read(1), "little")

    def read_u16_le(self) -> int:
        return int.from_bytes(self._read(2), "little")

    def read_u32_le(self) -> int:
        return int.from_bytes(self._read(4), "little")

    def read_u64_le(self) -> int:
        return int.from_bytes(self._read(8), "little")

    def read_bytes(self, length: int) -> bytes:
        return self._read(length)

    def skip(self, length: int) -> None:
        self._require(length)
        self._position += length

    def subspan(self, length: int) -> "ByteReader":
        """Consume ``length`` bytes and return them as an independent reader."""

        self._require(length)
        start = self._position
        end = start + length
        self._position = end
        return ByteReader(self._data, Span(start, end))

    def subspan_bounds(self, start: int, end: int) -> "ByteReader":
        """Return a child over ``[start, end)`` relative to this reader's span.

        The current cursor and parent span are unchanged.
        """

        if start < 0 or end < start or end > self._span.length:
            raise ValueError("subspan bounds must lie within the parent span")
        absolute_start = self._span.start + start
        absolute_end = self._span.start + end
        return ByteReader(self._data, Span(absolute_start, absolute_end))

    def remaining_bytes(self) -> bytes:
        return self._data[self._position : self._span.end]

    def _require(self, length: int) -> None:
        if length < 0:
            raise ValueError("read length must be non-negative")
        available = self._span.end - self._position
        if length > available:
            raise BoundedReadError(
                DecodeIssue(
                    code=IssueCode.TRUNCATED,
                    message=f"requested {length} bytes, only {available} available",
                    absolute_offset=self._position,
                    required=length,
                    available=available,
                )
            )

    def _read(self, length: int) -> bytes:
        self._require(length)
        start = self._position
        self._position += length
        return self._data[start : self._position]


@dataclass(frozen=True)
class RawCapture:
    """Immutable raw bytes plus the context required by later parsing layers."""

    data: bytes
    context: CaptureContext

    def __post_init__(self) -> None:
        if not isinstance(self.context, CaptureContext):
            raise TypeError("context must be a CaptureContext")
        object.__setattr__(self, "data", bytes(self.data))

    def reader(self, span: Optional[Span] = None) -> ByteReader:
        return ByteReader(self.data, span)
