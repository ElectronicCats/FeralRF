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
    uuids_16bit: list[str] = field(default_factory=list)
    uuids_128bit: list[str] = field(default_factory=list)
    services_uuid16_data: dict[str, bytes] = field(default_factory=dict)
    manufacturer_data: dict[int, bytes] = field(default_factory=dict)
    tx_power: Optional[int] = None
    appearance: Optional[int] = None
    raw_advs: list[bytes] = field(default_factory=list)
    raw_scan_rsps: list[bytes] = field(default_factory=list)


def parse_ad_structures(payload: bytes) -> dict:
    """Parse BLE advertising data structures.

    Returns a dict with keys present only for AD types found in payload.
    Malformed length fields and unknown AD types are skipped silently.
    Never raises.
    """
    out: dict = {}
    i = 0
    n = len(payload)
    while i < n:
        ad_len = payload[i]
        if ad_len == 0:
            i += 1
            continue
        if i + 1 + ad_len > n:
            break  # truncated
        ad_type = payload[i + 1]
        value = payload[i + 2 : i + 1 + ad_len]

        if ad_type == 0x01 and len(value) >= 1:
            out["flags"] = value[0]
        elif ad_type == 0x0A and len(value) >= 1:
            out["tx_power"] = int.from_bytes(value[:1], "little", signed=True)
        elif ad_type == 0x19 and len(value) >= 2:
            out["appearance"] = int.from_bytes(value[:2], "little")
        elif ad_type == 0x09:
            out["name"] = value.decode("utf-8", errors="replace")
        elif ad_type == 0x08:
            if "name" not in out:
                out["name"] = value.decode("utf-8", errors="replace")
        elif ad_type in (0x02, 0x03):
            uuids = out.setdefault("uuids_16bit", [])
            for j in range(0, len(value) - 1, 2):
                uuid_int = int.from_bytes(value[j : j + 2], "little")
                uuids.append(f"{uuid_int:04X}")
        elif ad_type in (0x06, 0x07):
            uuids = out.setdefault("uuids_128bit", [])
            for j in range(0, len(value) - 15, 16):
                rev = value[j : j + 16][::-1]
                hex_str = rev.hex()
                canonical = (
                    f"{hex_str[0:8]}-{hex_str[8:12]}-{hex_str[12:16]}-"
                    f"{hex_str[16:20]}-{hex_str[20:32]}"
                )
                uuids.append(canonical)
        elif ad_type == 0x16 and len(value) >= 2:
            uuid_int = int.from_bytes(value[:2], "little")
            uuid_str = f"{uuid_int:04X}"
            data = bytes(value[2:])
            sd = out.setdefault("services_uuid16_data", {})
            sd[uuid_str] = data
        elif ad_type == 0xFF and len(value) >= 2:
            company_id = int.from_bytes(value[:2], "little")
            data = bytes(value[2:])
            md = out.setdefault("manufacturer_data", {})
            md[company_id] = data

        i += 1 + ad_len
    return out
