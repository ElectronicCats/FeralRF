"""FeralRF — GATT hardware-in-the-loop integration test.

Run with:
    FERALRF_PERIPHERAL_ADDR=AA:BB:CC:DD:EE:FF \\
    FERALRF_PERIPHERAL_TYPE=1 \\
    pytest tests/test_gatt_integration.py -m hardware_ble -v

Skipped unless the env vars are set.
"""

import os
import time
from typing import Generator

import pytest

from feralrf import Radio

pytestmark = pytest.mark.hardware_ble


ADDR: str = os.environ.get("FERALRF_PERIPHERAL_ADDR") or ""
ATYPE = int(os.environ.get("FERALRF_PERIPHERAL_TYPE", "1"))

if not ADDR:
    pytest.skip("FERALRF_PERIPHERAL_ADDR not set", allow_module_level=True)


def _addr_le(s: str) -> bytes:
    return bytes(int(x, 16) for x in reversed(s.split(":")))


@pytest.fixture(scope="module")
def radio() -> Generator[Radio, None, None]:
    r = Radio()
    r.init()
    yield r
    try:
        r.ble_disconnect(timeout=1.0)
    except Exception:
        pass
    r.disconnect()


def test_connect_discover_and_read_device_name(radio: Radio):
    result = radio.ble_connect(_addr_le(ADDR), ATYPE, timeout=8.0)
    assert result.is_ok, f"connect failed: result={result.result}"

    time.sleep(1.5)
    status = radio.conn_status()
    assert status.connected, f"connection dropped before discovery: {status}"

    discovery = radio.gatt_discover(timeout=20.0)
    assert len(discovery.services) >= 1, "no services discovered"
    assert len(discovery.characteristics) >= 1, "no characteristics discovered"
    assert discovery.status == 0, f"discovery status={discovery.status}"

    # Prefer Device Name (0x2A00) if present; else first readable char.
    target = None
    for ch in discovery.characteristics:
        if ch.uuid == b"\x00\x2A" and (ch.properties & 0x02):
            target = ch
            break
    if target is None:
        for ch in discovery.characteristics:
            if ch.properties & 0x02:
                target = ch
                break
    assert target is not None, "no readable characteristic found"

    value = radio.gatt_read(target.value_handle, timeout=5.0)
    assert len(value) > 0, "empty read value"


def test_disconnect_is_clean_and_allows_reconnect(radio: Radio):
    radio.ble_disconnect(timeout=2.0)

    # Second connect must succeed without a reset_device() in between.
    result = radio.ble_connect(_addr_le(ADDR), ATYPE, timeout=8.0)
    assert result.is_ok, f"reconnect failed: result={result.result}"
    radio.ble_disconnect(timeout=2.0)
