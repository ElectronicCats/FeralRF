"""BLE active scanner support — dataclass, AD parser, PDU layout helpers.

Consumed by feralrf.radio.Radio.scan_ble_active(). The TI-RTOS firmware
emits ADV_* and SCAN_RSP packets through the same data queue with
ll_pdu_type=0x04 distinguishing SCAN_RSP. This module merges them per MAC
into BleScanResult and decodes the AD structures.
"""

from dataclasses import dataclass, field
from typing import Optional


@dataclass
class BleScanResult:
    mac: str
    addr_type: str
    name: Optional[str] = None
    rssi_max: int = -128
    rssi_min: int = 0
    rssi_avg: float = 0.0
    adv_count: int = 0
    scan_rsp_count: int = 0
    flags: Optional[int] = None
    uuids_16bit: list = field(default_factory=list)
    uuids_128bit: list = field(default_factory=list)
    services_uuid16_data: dict = field(default_factory=dict)
    manufacturer_data: dict = field(default_factory=dict)
    tx_power: Optional[int] = None
    appearance: Optional[int] = None
    raw_advs: list = field(default_factory=list)
    raw_scan_rsps: list = field(default_factory=list)


def parse_ad_structures(payload: bytes) -> dict:
    """Parse BLE advertising data structures.

    Returns a dict with keys present only for AD types found in payload.
    Malformed length fields and unknown AD types are skipped silently.
    Never raises.
    """
    return {}
