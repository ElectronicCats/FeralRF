"""Tests for the explicit Packet-to-RawCapture adapter."""

import subprocess
import sys
import textwrap
from dataclasses import FrozenInstanceError
from pathlib import Path

import pytest

from feralrf.enums import PHY
from feralrf.radio import Packet
from feralrf.zigbee.capture import capture_from_packet
from feralrf.zigbee.core import (
    CaptureContext,
    FieldPresence,
    IssueCode,
    Provenance,
    SchemaProfile,
)


def test_capture_from_packet_preserves_raw_bytes_and_packet_metadata():
    raw = b"\x05\xaa\xbb\xcc\xdd"
    packet = Packet(
        timestamp_us=1_234_567,
        channel=15,
        rssi_dbm=-73,
        lqi=201,
        crc_ok=True,
        data=raw,
    )

    capture = capture_from_packet(
        packet,
        CaptureContext(
            phy=PHY.IEEE_802_15_4,
            phr=FieldPresence.ABSENT,
            fcs=FieldPresence.PRESENT,
        ),
    )

    assert capture.data == raw
    assert capture.data[:1] == b"\x05"
    assert capture.data[-2:] == b"\xcc\xdd"
    assert len(capture.data) == len(raw)
    assert capture.timestamp_us == 1_234_567
    assert capture.channel == 15
    assert capture.rssi_dbm == -73
    assert capture.lqi == 201
    assert capture.crc_ok is True


def test_capture_from_packet_copies_mutable_packet_data():
    source = bytearray(b"\x01\x02\x03")
    packet = Packet(
        timestamp_us=1,
        channel=20,
        rssi_dbm=-40,
        lqi=100,
        crc_ok=True,
        data=source,  # type: ignore[arg-type]
    )

    capture = capture_from_packet(packet, CaptureContext())
    source[0] = 0xFF

    assert capture.data == b"\x01\x02\x03"
    assert isinstance(capture.data, bytes)


def test_capture_preserves_explicit_context_as_an_immutable_snapshot():
    schema = SchemaProfile(
        name="zigbee-r22",
        ieee_802_15_4_revision="2015",
        zigbee_core_revision="R22",
    )
    context = CaptureContext(
        phy=PHY.IEEE_802_15_4,
        phr=FieldPresence.ABSENT,
        fcs=FieldPresence.PRESENT,
        provenance=Provenance.DERIVED_FROM_SESSION,
        schema=schema,
        session_epoch=42,
    )
    packet = Packet(10, 25, -55, 180, True, b"\xaa")

    capture = capture_from_packet(packet, context)

    assert capture.context == context
    assert capture.context.phy is PHY.IEEE_802_15_4
    assert capture.context.phr is FieldPresence.ABSENT
    assert capture.context.fcs is FieldPresence.PRESENT
    assert capture.context.provenance is Provenance.DERIVED_FROM_SESSION
    assert capture.context.schema == schema
    assert capture.context.session_epoch == 42
    with pytest.raises(FrozenInstanceError):
        capture.timestamp_us = 99
    with pytest.raises(FrozenInstanceError):
        capture.context.session_epoch = 99


def test_channel_does_not_supply_missing_phy_or_unknown_byte_policies():
    packet = Packet(10, 15, -55, 180, True, b"\xaa")

    capture = capture_from_packet(packet, CaptureContext())

    assert capture.channel == 15
    assert capture.context.phy is None
    assert capture.context.phr is FieldPresence.UNKNOWN
    assert capture.context.fcs is FieldPresence.UNKNOWN
    assert capture.context.provenance is Provenance.UNKNOWN
    assert capture.context.compatibility_issues(phy=PHY.IEEE_802_15_4)[
        0
    ].code is IssueCode.MISSING_CONTEXT


def test_capture_context_conflict_remains_explicit_after_adaptation():
    packet = Packet(10, 15, -55, 180, True, b"\xaa")
    capture = capture_from_packet(
        packet,
        CaptureContext(
            phy=PHY.IEEE_802_15_4,
            provenance=Provenance.DECLARED_BY_CALLER,
        ),
    )

    issues = capture.context.compatibility_issues(phy=PHY.BLE_1M)

    assert len(issues) == 1
    assert issues[0].code is IssueCode.CONTEXT_MISMATCH


def test_crc_bad_packet_remains_an_unmodified_raw_capture():
    raw = b"\x61\x88\x00\xde\xad"
    packet = Packet(99, 11, -90, 7, False, raw)

    capture = capture_from_packet(
        packet,
        CaptureContext(
            phy=PHY.IEEE_802_15_4,
            fcs=FieldPresence.UNKNOWN,
        ),
    )

    assert capture.data == raw
    assert capture.crc_ok is False
    assert capture.context.fcs is FieldPresence.UNKNOWN


def test_import_capture_adapter_has_no_hardware_side_effects():
    python_dir = Path(__file__).resolve().parents[1]
    script = textwrap.dedent(
        """
        import sys
        import serial
        import serial.tools.list_ports as list_ports

        import feralrf
        from feralrf import radio as radio_module


        def _fail(*args, **kwargs):
            raise AssertionError("hardware access during capture adapter import")


        serial.Serial = _fail
        list_ports.comports = _fail
        radio_module.Radio.__init__ = _fail

        import feralrf.zigbee.capture  # noqa: F401

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
