"""
Tests for Radio._get_shell_port() macOS-aware derivation.

macOS CDC-ACM devices enumerate as /dev/cu.usbmodem<serial><iface> — the
trailing digits are NOT a sequential index like Linux ttyACM*, so the old
"+2" heuristic produces a bogus port name on macOS. These tests cover the
new list_ports sibling-match strategy plus the preserved Linux fallback.
"""

import types

import pytest

from feralrf.exceptions import ConnectionError
from feralrf.radio import Radio


def _port(device, serial_number=None, location=None):
    return types.SimpleNamespace(device=device, serial_number=serial_number, location=location)


def test_shell_port_macos_sibling_match(monkeypatch):
    ports = [
        _port("/dev/cu.usbmodem1101", serial_number="E4B3", location="0-1.1:1.0"),  # bridge
        _port(
            "/dev/cu.usbmodem1103", serial_number="E4B3", location="0-1.1:1.2"
        ),  # shell (same serial)
        _port("/dev/cu.usbmodem9999", serial_number="ZZZZ", location="0-2:1.0"),  # unrelated
    ]
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: ports)
    r = Radio(port="/dev/cu.usbmodem1101")
    assert r._get_shell_port() == "/dev/cu.usbmodem1103"


def test_shell_port_linux_offset_fallback(monkeypatch):
    # ttyACM siblings share no serial_number -> fall back to +2
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: [])
    r = Radio(port="/dev/ttyACM0")
    assert r._get_shell_port() == "/dev/ttyACM2"


def test_shell_port_macos_sibling_match_not_offset_by_two(monkeypatch):
    # The offset between bridge and shell port numbers is +4 here, not +2,
    # so this only passes if the sibling-match strategy (not the numeric
    # fallback) is what produced the answer.
    ports = [
        _port("/dev/cu.usbmodem14201", serial_number="F00D", location="0-1.2:1.0"),  # bridge
        _port("/dev/cu.usbmodem14205", serial_number="F00D", location="0-1.2:1.2"),  # shell
    ]
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: ports)
    r = Radio(port="/dev/cu.usbmodem14201")
    assert r._get_shell_port() == "/dev/cu.usbmodem14205"


def test_shell_port_macos_sibling_match_via_location_only(monkeypatch):
    # No serial_number reported (some drivers omit it) -> match must fall
    # back to comparing the USB location path prefix instead.
    ports = [
        _port("/dev/cu.usbmodem9001", serial_number=None, location="0-3.1:1.0"),  # bridge
        _port("/dev/cu.usbmodem9099", serial_number=None, location="0-3.1:1.2"),  # shell
    ]
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: ports)
    r = Radio(port="/dev/cu.usbmodem9001")
    assert r._get_shell_port() == "/dev/cu.usbmodem9099"


def test_shell_port_macos_ambiguous_siblings_fall_back_to_offset(monkeypatch):
    # Two other CDC interfaces share the bridge's serial_number -> the
    # sibling match is ambiguous, so we must fall back to the +2 heuristic
    # instead of guessing wrong.
    ports = [
        _port("/dev/cu.usbmodem7700", serial_number="S9", location="0-4:1.0"),  # bridge
        _port("/dev/cu.usbmodem7702", serial_number="S9", location="0-4:1.2"),  # candidate 1
        _port("/dev/cu.usbmodem7704", serial_number="S9", location="0-4:1.4"),  # candidate 2
    ]
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: ports)
    r = Radio(port="/dev/cu.usbmodem7700")
    assert r._get_shell_port() == "/dev/cu.usbmodem7702"


def test_shell_port_raises_when_undeterminable(monkeypatch):
    monkeypatch.setattr("serial.tools.list_ports.comports", lambda: [])
    r = Radio(port="/dev/cu.usbmodemXYZ")
    with pytest.raises(ConnectionError):
        r._get_shell_port()
