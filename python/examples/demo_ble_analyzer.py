#!/usr/bin/env python3
"""
FeralRF BLE Device Analyzer

Deep analysis of BLE advertising devices. Captures, decodes, and reports
everything about nearby BLE devices.

Flow:
  1. Baseline scan (target OFF) — identifies ambient devices
  2. Target scan (target ON) — finds new devices
  3. Deep analysis of each new device
  4. Full report printed + saved to JSON file

Reports:
  - MAC address (display + raw), address type
  - All AD structures decoded (Flags, Name, Manufacturer, UUIDs, Service Data)
  - Company identification (Apple, Google, Samsung, Anker, etc.)
  - Fast Pair / Apple Proximity detection
  - RSSI statistics (min/avg/max)
  - Advertising rate and timing
  - PDU types seen
  - Channel distribution
  - Vulnerability assessment and attack recommendations

Usage:
    python demo_ble_analyzer.py [port] [output.json]

All steps are interactive — press Enter to advance.
"""

import sys
import time
import json
from collections import defaultdict
from feralrf import Radio, PHY

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
OUTPUT = sys.argv[2] if len(sys.argv) > 2 else "ble_analysis.json"

# ─── Known Identifiers ───

COMPANIES = {
    0x004C: "Apple", 0x0006: "Microsoft", 0x00E0: "Google",
    0x0075: "Samsung", 0x2BF4: "Anker", 0x000D: "Texas Instruments",
    0x0059: "Nordic Semiconductor", 0x0131: "Cypress", 0x02FF: "Qualcomm",
    0x0171: "Amazon", 0x038F: "Xiaomi", 0x0157: "Huawei",
    0x0087: "Garmin", 0x00D2: "Bose", 0x012D: "Sony",
    0x0310: "Jabra", 0x0056: "Sony Ericsson", 0x0046: "MediaTek",
}

UUIDS16 = {
    0xFE2C: "Google Fast Pair", 0xFCF1: "Google Nearby Share",
    0xFE9F: "Google Eddystone", 0xFD6F: "Apple Exposure Notification",
    0xFEAA: "Google Eddystone", 0xFE07: "Sonos",
    0x180A: "Device Information", 0x1812: "Human Interface Device",
    0x180F: "Battery Service", 0x1800: "Generic Access",
    0x1801: "Generic Attribute", 0x181A: "Environmental Sensing",
    0xFFF0: "Custom/Vendor", 0xFFF1: "Custom/Vendor",
}

PDU_TYPES = {
    0: "ADV_IND", 1: "ADV_DIRECT_IND", 2: "ADV_NONCONN_IND",
    3: "SCAN_REQ", 4: "SCAN_RSP", 5: "CONNECT_IND", 6: "ADV_SCAN_IND",
}

AD_TYPE_NAMES = {
    0x01: "Flags", 0x02: "16-bit UUID (incomplete)", 0x03: "16-bit UUID (complete)",
    0x04: "32-bit UUID (incomplete)", 0x05: "32-bit UUID (complete)",
    0x06: "128-bit UUID (incomplete)", 0x07: "128-bit UUID (complete)",
    0x08: "Shortened Local Name", 0x09: "Complete Local Name",
    0x0A: "TX Power Level", 0x0D: "Class of Device",
    0x0E: "Simple Pairing Hash", 0x0F: "Simple Pairing Randomizer",
    0x10: "Security Manager TK", 0x11: "Security Manager OOB Flags",
    0x12: "Peripheral Connection Interval", 0x14: "16-bit Solicitation UUID",
    0x15: "128-bit Solicitation UUID", 0x16: "Service Data (16-bit)",
    0x17: "Public Target Address", 0x18: "Random Target Address",
    0x19: "Appearance", 0x1A: "Advertising Interval",
    0x1B: "LE Bluetooth Device Address", 0x1C: "LE Role",
    0x20: "Service Data (32-bit)", 0x21: "Service Data (128-bit)",
    0xFF: "Manufacturer Specific Data",
}

FLAG_BITS = {
    0x01: "LE Limited Discoverable",
    0x02: "LE General Discoverable",
    0x04: "BR/EDR Not Supported",
    0x08: "LE+BR/EDR Controller",
    0x10: "LE+BR/EDR Host",
}

APPEARANCES = {
    0x0000: "Unknown", 0x0040: "Phone", 0x0080: "Computer",
    0x00C0: "Watch", 0x00C1: "Sports Watch", 0x0100: "Clock",
    0x0140: "Display", 0x0180: "Remote Control",
    0x01C0: "Eye Glasses", 0x0200: "Tag", 0x0240: "Keyring",
    0x0280: "Media Player", 0x02C0: "Barcode Scanner",
    0x0300: "Thermometer", 0x0340: "Heart Rate Sensor",
    0x03C0: "Cycling", 0x0440: "Pulse Oximeter",
    0x04C0: "Weight Scale", 0x0540: "Outdoor Sports",
    0x0841: "Keyboard", 0x0842: "Mouse", 0x0843: "Joystick",
    0x0844: "Gamepad", 0x0CC1: "Hearing Aid",
}


def wait(msg):
    input(f"\n>>> {msg}")


def decode_ad(ad_type, ad_data):
    """Decode a single AD structure into a dict."""
    result = {
        "type": ad_type,
        "type_hex": f"0x{ad_type:02X}",
        "type_name": AD_TYPE_NAMES.get(ad_type, f"Unknown (0x{ad_type:02X})"),
        "raw": ad_data.hex(),
        "length": len(ad_data),
    }

    if ad_type == 0x01 and ad_data:
        flags = ad_data[0]
        result["flags"] = flags
        result["flags_str"] = [name for bit, name in FLAG_BITS.items() if flags & bit]

    elif ad_type in (0x08, 0x09) and ad_data:
        result["name"] = ad_data.decode("utf-8", errors="replace")

    elif ad_type == 0x0A and ad_data:
        result["tx_power_dbm"] = int.from_bytes(ad_data[:1], "little", signed=True)

    elif ad_type == 0xFF and len(ad_data) >= 2:
        cid = ad_data[0] | (ad_data[1] << 8)
        result["company_id"] = cid
        result["company_hex"] = f"0x{cid:04X}"
        result["company_name"] = COMPANIES.get(cid, "Unknown")
        result["mfr_data"] = ad_data[2:].hex()

        if cid == 0x004C and len(ad_data) >= 4:
            apple_type = ad_data[2]
            result["apple_type"] = apple_type
            if apple_type == 0x07:
                result["apple_protocol"] = "Proximity Pairing"
                result["vulnerability"] = "iOS popup spoofing possible"
            elif apple_type == 0x09:
                result["apple_protocol"] = "AirPlay"
            elif apple_type == 0x10:
                result["apple_protocol"] = "Nearby Action"
            elif apple_type == 0x12:
                result["apple_protocol"] = "FindMy"

    elif ad_type == 0x16 and len(ad_data) >= 2:
        uuid16 = ad_data[0] | (ad_data[1] << 8)
        result["uuid16"] = uuid16
        result["uuid16_hex"] = f"0x{uuid16:04X}"
        result["uuid16_name"] = UUIDS16.get(uuid16, "Unknown")
        result["service_data"] = ad_data[2:].hex()

        if uuid16 == 0xFE2C:
            data = ad_data[2:]
            if len(data) == 3:
                result["fastpair_mode"] = "discoverable"
                result["fastpair_model_id"] = f"0x{data.hex().upper()}"
                result["vulnerability"] = "Fast Pair popup can be replicated"
            elif len(data) > 3:
                result["fastpair_mode"] = "not-discoverable"
                result["fastpair_filter"] = data[:4].hex()

    elif ad_type in (0x02, 0x03) and len(ad_data) >= 2:
        uuids = []
        for j in range(0, len(ad_data) - 1, 2):
            u = ad_data[j] | (ad_data[j + 1] << 8)
            uuids.append({"uuid": f"0x{u:04X}", "name": UUIDS16.get(u, "Unknown")})
        result["uuids"] = uuids

    elif ad_type in (0x04, 0x05) and len(ad_data) >= 4:
        uuids = []
        for j in range(0, len(ad_data) - 3, 4):
            u = int.from_bytes(ad_data[j:j + 4], "little")
            uuids.append(f"0x{u:08X}")
        result["uuids"] = uuids

    elif ad_type == 0x19 and len(ad_data) >= 2:
        appearance = ad_data[0] | (ad_data[1] << 8)
        result["appearance"] = appearance
        result["appearance_name"] = APPEARANCES.get(appearance, f"Unknown (0x{appearance:04X})")

    return result


def analyze_device(mac, info, start_time):
    """Full analysis of a captured device."""
    mac_parts = [mac[i:i + 2] for i in range(0, 12, 2)]
    mac_display = ":".join(reversed(mac_parts)).upper()
    mac_raw = ":".join(mac_parts).upper()

    # Address type from MSB
    first_byte = int(mac_parts[5], 16) if mac_parts else 0
    if first_byte & 0xC0 == 0xC0:
        addr_type = "Random Static"
    elif first_byte & 0xC0 == 0x40:
        addr_type = "Resolvable Private"
    elif first_byte & 0xC0 == 0x00:
        addr_type = "Non-Resolvable Private"
    else:
        addr_type = "Public"

    rssi_avg = info["rssi_sum"] / info["count"] if info["count"] > 0 else 0
    duration = info["last_seen"] - info["first_seen"] if info["last_seen"] != info["first_seen"] else 0
    adv_rate = info["count"] / duration if duration > 0 else 0

    device = {
        "mac_display": mac_display,
        "mac_raw": mac_raw,
        "address_type": addr_type,
        "packets": info["count"],
        "rssi": {
            "min": info["rssi_min"],
            "avg": round(rssi_avg, 1),
            "max": info["rssi_max"],
        },
        "channels": sorted(info["channels"]),
        "duration_s": round(duration, 1),
        "adv_rate_pps": round(adv_rate, 1),
        "adv_interval_ms": round(1000 / adv_rate, 0) if adv_rate > 0 else None,
        "pdu_types": sorted(info["pdu_types"]),
        "payloads": [],
        "device_name": None,
        "manufacturer": None,
        "has_fastpair": False,
        "has_apple": False,
        "vulnerabilities": [],
    }

    for ph, pcount in info["payloads"].items():
        payload = bytes.fromhex(ph)
        pdu_byte = info["payload_pdu"].get(ph, 0)
        pdu_name = PDU_TYPES.get(pdu_byte & 0x0F, f"Unknown(0x{pdu_byte:02X})")

        payload_info = {
            "count": pcount,
            "size": len(payload),
            "pdu_type": pdu_name,
            "raw_hex": ph,
            "ad_structures": [],
        }

        # Decode AD structures
        i = 0
        while i < len(payload) - 1:
            al = payload[i]
            if al == 0 or i + al >= len(payload):
                break
            at = payload[i + 1]
            ad = payload[i + 2:i + 1 + al]
            decoded = decode_ad(at, ad)
            payload_info["ad_structures"].append(decoded)

            # Extract device-level info
            if "name" in decoded:
                device["device_name"] = decoded["name"]
            if "company_name" in decoded and decoded["company_name"] != "Unknown":
                device["manufacturer"] = decoded["company_name"]
            if "fastpair_mode" in decoded:
                device["has_fastpair"] = True
                if decoded["fastpair_mode"] == "discoverable":
                    device["fastpair_model_id"] = decoded["fastpair_model_id"]
            if "apple_protocol" in decoded:
                device["has_apple"] = True
                device["apple_protocol"] = decoded["apple_protocol"]
            if "vulnerability" in decoded:
                device["vulnerabilities"].append(decoded["vulnerability"])

            i += al + 1

        device["payloads"].append(payload_info)

    # General vulnerabilities
    if not any("encrypted" in str(p).lower() for p in device["payloads"]):
        device["vulnerabilities"].append("Advertising is unencrypted — replay/clone possible")
    if device["adv_interval_ms"] and device["adv_interval_ms"] < 200:
        device["vulnerabilities"].append(f"Fast advertising ({device['adv_interval_ms']:.0f}ms) — easy to capture")

    return device


def print_device(dev, index):
    """Pretty-print device analysis."""
    print(f"\n{'='*70}")
    print(f"  DEVICE #{index}: {dev['mac_display']}")
    print(f"{'='*70}")
    print(f"  MAC (raw):      {dev['mac_raw']}")
    print(f"  Address Type:   {dev['address_type']}")
    print(f"  Name:           {dev['device_name'] or '(none)'}")
    print(f"  Manufacturer:   {dev['manufacturer'] or 'Unknown'}")
    print(f"  Packets:        {dev['packets']}")
    print(f"  RSSI:           min={dev['rssi']['min']} avg={dev['rssi']['avg']} max={dev['rssi']['max']} dBm")
    print(f"  Channels:       {dev['channels']}")
    print(f"  Duration:       {dev['duration_s']}s")
    if dev["adv_interval_ms"]:
        print(f"  ADV Rate:       {dev['adv_rate_pps']} pkt/s (~{dev['adv_interval_ms']:.0f}ms interval)")
    print(f"  PDU Types:      {dev['pdu_types']}")
    print(f"  Fast Pair:      {'Yes' if dev['has_fastpair'] else 'No'}", end="")
    if dev.get("fastpair_model_id"):
        print(f" (Model ID: {dev['fastpair_model_id']})", end="")
    print()
    print(f"  Apple:          {'Yes (' + dev.get('apple_protocol', '') + ')' if dev['has_apple'] else 'No'}")

    print(f"\n  --- Payloads ({len(dev['payloads'])}) ---")
    for j, p in enumerate(dev["payloads"]):
        print(f"\n  Payload #{j + 1} ({p['count']}x, {p['size']}B, {p['pdu_type']}):")
        print(f"    Raw: {p['raw_hex']}")
        for ad in p["ad_structures"]:
            line = f"    • {ad['type_name']}"
            if "name" in ad:
                line += f': "{ad["name"]}"'
            elif "company_name" in ad:
                line += f": {ad['company_name']} (0x{ad['company_id']:04X}) data={ad['mfr_data']}"
            elif "uuid16_name" in ad:
                line += f": {ad['uuid16_name']} ({ad['uuid16_hex']})"
                if "fastpair_mode" in ad:
                    line += f" [{ad['fastpair_mode']}]"
                    if ad["fastpair_mode"] == "discoverable":
                        line += f" Model={ad['fastpair_model_id']}"
                if "service_data" in ad:
                    line += f" data={ad['service_data']}"
            elif "tx_power_dbm" in ad:
                line += f": {ad['tx_power_dbm']} dBm"
            elif "flags_str" in ad:
                line += f": {', '.join(ad['flags_str'])}"
            elif "appearance_name" in ad:
                line += f": {ad['appearance_name']}"
            elif "uuids" in ad:
                if isinstance(ad["uuids"][0], dict):
                    line += ": " + ", ".join(f"{u['uuid']} ({u['name']})" for u in ad["uuids"])
                else:
                    line += ": " + ", ".join(ad["uuids"])
            else:
                line += f": {ad['raw']}"
            print(line)

    if dev["vulnerabilities"]:
        print(f"\n  --- Vulnerabilities ---")
        for v in dev["vulnerabilities"]:
            print(f"    ⚠ {v}")


# ─── MAIN ───

print("=" * 70)
print("  FeralRF BLE Device Analyzer")
print("=" * 70)
print()
print("  This tool analyzes BLE advertising of nearby devices.")
print("  It captures all AD structures, identifies protocols,")
print("  detects vulnerabilities, and exports a full report.")
print()
print(f"  Port: {PORT}")
print(f"  Output: {OUTPUT}")

radio = Radio(PORT)
radio.connect()
time.sleep(1)
radio.init()

# Phase 1
wait("Make sure target device is OFF. Press Enter to scan baseline...")
print("Scanning baseline (15s)...")
radio.set_phy(PHY.BLE_1M, channel=37)
radio.set_adv_hop(True)
radio.start_rx()
baseline = set()
for pkt in radio.read_packets(timeout=15.0):
    if pkt.crc_ok and len(pkt.data) > 8:
        baseline.add(pkt.data[2:8].hex())
radio.stop_rx()
time.sleep(0.3)
print(f"Baseline: {len(baseline)} ambient devices")

# Phase 2
wait("Turn ON the target device. Press Enter to scan...")
print("Scanning with target (30s)...")
radio.start_rx()
devices = defaultdict(lambda: {
    "count": 0, "rssi_min": 0, "rssi_max": -128, "rssi_sum": 0,
    "channels": set(), "pdu_types": set(), "payloads": {},
    "payload_pdu": {}, "first_seen": None, "last_seen": None,
})
start_time = time.time()
for pkt in radio.read_packets(timeout=30.0):
    if pkt.crc_ok and len(pkt.data) > 8:
        mac = pkt.data[2:8].hex()
        now = time.time() - start_time
        d = devices[mac]
        d["count"] += 1
        d["rssi_min"] = min(d["rssi_min"], pkt.rssi_dbm)
        d["rssi_max"] = max(d["rssi_max"], pkt.rssi_dbm)
        d["rssi_sum"] += pkt.rssi_dbm
        d["channels"].add(pkt.channel & 0x3F)
        d["pdu_types"].add(PDU_TYPES.get(pkt.data[0] & 0x0F, f"0x{pkt.data[0]:02X}"))
        if d["first_seen"] is None:
            d["first_seen"] = now
        d["last_seen"] = now
        ph = pkt.data[8:].hex()
        if ph not in d["payloads"]:
            d["payloads"][ph] = 0
            d["payload_pdu"][ph] = pkt.data[0]
        d["payloads"][ph] += 1
radio.stop_rx()
radio.disconnect()

# Phase 3: Analysis
new_macs = set(devices.keys()) - baseline
if not new_macs:
    print("\nNo new devices found. Try again closer to the target.")
    sys.exit(1)

sorted_macs = sorted(new_macs, key=lambda m: devices[m]["rssi_max"], reverse=True)
analyzed = []
for i, mac in enumerate(sorted_macs):
    dev = analyze_device(mac, devices[mac], start_time)
    analyzed.append(dev)
    print_device(dev, i + 1)

# Export
report = {
    "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
    "baseline_count": len(baseline),
    "new_devices": len(analyzed),
    "devices": analyzed,
}

with open(OUTPUT, "w") as f:
    json.dump(report, f, indent=2, default=str)

print(f"\n{'='*70}")
print(f"  Report saved to: {OUTPUT}")
print(f"  Devices analyzed: {len(analyzed)}")
print(f"{'='*70}")
