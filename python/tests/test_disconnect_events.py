"""F8c — async disconnect event iterator (no hardware)."""

from __future__ import annotations

import os
import sys

from feralrf.radio import DisconnectEvent, Radio


def test_disconnect_event_dataclass():
    e = DisconnectEvent(reason=0x13, timestamp=1234.5)
    assert e.reason == 0x13
    assert e.timestamp == 1234.5


def test_radio_has_read_disconnect_events_method():
    assert hasattr(Radio, "read_disconnect_events")


def test_disconnect_event_reason_label_is_human_readable():
    # Optional — reason byte → BT spec label.
    e = DisconnectEvent(reason=0x13, timestamp=0.0)
    assert "REMOTE_USER_TERMINATED" in e.reason_label

    e = DisconnectEvent(reason=0x16, timestamp=0.0)
    assert "LOCAL_HOST_TERMINATED" in e.reason_label

    e = DisconnectEvent(reason=0x22, timestamp=0.0)
    assert "RESPONSE_TIMEOUT" in e.reason_label

    e = DisconnectEvent(reason=0xFF, timestamp=0.0)
    # Unknown reasons fall back to a hex string.
    assert "0xFF" in e.reason_label or "UNKNOWN" in e.reason_label


# --- Behavior tests with FakeSerial ---

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from test_gatt_api import _radio_with_fake_serial  # noqa: E402

from feralrf.enums import Response  # noqa: E402


def test_read_disconnect_events_yields_event_with_reason():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.DISCONNECTED, seq=0, payload=b"\x16")

    events = list(radio.read_disconnect_events(timeout=0.5))

    assert len(events) == 1
    assert events[0].reason == 0x16
    assert events[0].reason_label == "LOCAL_HOST_TERMINATED"


def test_read_disconnect_events_returns_quietly_on_empty_buffer():
    """No queued frames → iterator ends immediately on timeout, no exception."""
    radio, fake = _radio_with_fake_serial()

    events = list(radio.read_disconnect_events(timeout=0.1))

    assert events == []


def test_read_disconnect_events_yields_multiple_events():
    radio, fake = _radio_with_fake_serial()
    fake.queue_response(Response.DISCONNECTED, seq=0, payload=b"\x13")
    fake.queue_response(Response.DISCONNECTED, seq=0, payload=b"\x22")

    events = list(radio.read_disconnect_events(timeout=0.5))

    assert len(events) == 2
    assert events[0].reason == 0x13
    assert events[1].reason == 0x22
