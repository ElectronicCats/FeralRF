#!/usr/bin/env python3
"""
BLE Device Cloner — Interactive, user-paced

Each step waits for you to press Enter before continuing.

Usage:
    python demo_ble_clone.py [port]
"""

import os
import sys
import time
from collections import defaultdict

from feralrf import PHY, Radio

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"

COMPANIES = {
    0x004C: "Apple",
    0x0006: "Microsoft",
    0x00E0: "Google",
    0x0075: "Samsung",
    0x2BF4: "Anker",
    0x000D: "TI",
}


def wait(msg="Press Enter to continue..."):
    input(f"\n>>> {msg}")


def scan(radio, duration=15):
    """Scan and return dict of {mac: {count, rssi_max, payloads}}."""
    radio.set_phy(PHY.BLE_1M, channel=37)
    radio.set_adv_hop(True)
    radio.start_rx()
    devs = defaultdict(lambda: {"count": 0, "rssi_max": -128, "payloads": {}})
    for pkt in radio.read_packets(timeout=duration):
        if pkt.crc_ok and len(pkt.data) > 8:
            mac = pkt.data[2:8].hex()
            d = devs[mac]
            d["count"] += 1
            d["rssi_max"] = max(d["rssi_max"], pkt.rssi_dbm)
            ph = pkt.data[8:].hex()
            if ph not in d["payloads"]:
                d["payloads"][ph] = 0
            d["payloads"][ph] += 1
    radio.stop_rx()
    time.sleep(0.3)
    return devs


def emit_until_enter(radio, mac_str, payload, power=5):
    """Transmit continuously until user presses Enter."""
    radio.set_ble_addr_str(mac_str)
    radio.set_phy(PHY.BLE_1M, channel=37)

    import threading

    stop = threading.Event()
    count = [0]

    def tx_loop():
        while not stop.is_set():
            try:
                radio.transmit(payload, power_dbm=power)
                count[0] += 1
                time.sleep(0.02)
            except Exception:
                time.sleep(0.1)

    t = threading.Thread(target=tx_loop, daemon=True)
    t.start()
    input("    Transmitting... Press Enter to stop.")
    stop.set()
    t.join(timeout=2)
    print(f"    Sent {count[0]} pkts")
    time.sleep(0.5)


# ─── START ───

radio = Radio(PORT)
radio.connect()
time.sleep(1)
radio.init()

print("=" * 60)
print("  BLE DEVICE CLONER — Interactive")
print("=" * 60)

# ─── STEP 1 ───
wait("Make sure target device is OFF, then press Enter to scan baseline...")
print("Scanning baseline (15s)...")
baseline_devs = scan(radio, 15)
baseline_macs = set(baseline_devs.keys())
print(f"Baseline: {len(baseline_macs)} devices")

# ─── STEP 2 ───
wait("Turn ON the target device, then press Enter to scan...")
print("Scanning for target (20s)...")
target_devs = scan(radio, 20)

new_macs = set(target_devs.keys()) - baseline_macs
if not new_macs:
    print("No new devices found! Try again.")
    radio.disconnect()
    sys.exit(1)

# Show new devices
print(f"\nNew devices: {len(new_macs)}")
sorted_new = sorted(new_macs, key=lambda m: target_devs[m]["rssi_max"], reverse=True)
for i, mac in enumerate(sorted_new[:5]):
    d = target_devs[mac]
    ms = ":".join(mac[j : j + 2] for j in range(0, 12, 2)).upper()
    print(f"  {i+1}. {ms}  RSSI={d['rssi_max']:3d}dBm  pkts={d['count']}")

# Auto-pick strongest
target_mac = sorted_new[0]
target = target_devs[target_mac]
# Raw capture is LSB-first, reverse to nRF Connect display order (MSB-first)
mac_parts = [target_mac[i : i + 2] for i in range(0, 12, 2)]
mac_str = ":".join(reversed(mac_parts)).upper()
mac_raw_str = ":".join(mac_parts).upper()
print(f"\nSelected: {mac_str} (nRF Connect display)")
print(f"  Raw capture: {mac_raw_str}")

# Analyze
target_payloads = [(bytes.fromhex(ph), cnt) for ph, cnt in target["payloads"].items()]
device_name = None
has_fastpair = False
fastpair_prefix = None
manufacturer_company = None

for p, _ in target_payloads:
    i = 0
    while i < len(p) - 1:
        al = p[i]
        if al == 0 or i + al >= len(p):
            break
        at = p[i + 1]
        ad = p[i + 2 : i + 1 + al]
        if at in (0x08, 0x09) and ad:
            device_name = ad.decode("utf-8", errors="replace")
        if at == 0xFF and len(ad) >= 2:
            manufacturer_company = ad[0] | (ad[1] << 8)
        if at == 0x16 and len(ad) >= 2:
            uuid16 = ad[0] | (ad[1] << 8)
            if uuid16 == 0xFE2C and len(ad) >= 5:
                has_fastpair = True
                fastpair_prefix = ad[2:5].hex()
        i += al + 1

company_name = (
    COMPANIES.get(manufacturer_company, f"0x{manufacturer_company:04X}")
    if manufacturer_company
    else "?"
)
print(f"  Name: {device_name or '(none)'}")
print(f"  Manufacturer: {company_name}")
print(f"  Fast Pair: {'Yes (prefix=' + fastpair_prefix + ')' if has_fastpair else 'No'}")
print(f"  Payloads: {len(target_payloads)}")
for p, c in target_payloads:
    print(f"    ({c}x, {len(p)}B): {p.hex()[:60]}")

# ─── STEP 3: Clone strategies ───
wait("Turn OFF the real device to avoid interference, then press Enter...")

# Build all strategies
strategies = []

# A) Exact clone
primary = max(target_payloads, key=lambda x: x[1])
strategies.append(("Exact clone (primary payload)", mac_str, primary[0]))

# B) With standard discoverable flags
payload_b = bytearray([0x02, 0x01, 0x06])
for p, _ in target_payloads:
    i = 0
    while i < len(p) - 1:
        al = p[i]
        if al == 0 or i + al >= len(p):
            break
        at = p[i + 1]
        if at != 0x01:
            chunk = p[i : i + 1 + al]
            if len(payload_b) + len(chunk) <= 31:
                payload_b += chunk
        i += al + 1
    break
if len(payload_b) > 3:
    strategies.append(("Modified flags (0x06)", mac_str, bytes(payload_b)))

# C) With device name
guess_name = device_name or {0x2BF4: "Soundcore Boom 2", 0x004C: "iPhone"}.get(
    manufacturer_company, "Device"
)
payload_c = bytearray([0x02, 0x01, 0x06])
nb = guess_name.encode("utf-8")[:20]
payload_c += bytes([len(nb) + 1, 0x09]) + nb
strategies.append((f'With name "{guess_name}"', mac_str, bytes(payload_c)))

# D) Name + manufacturer data
payload_d = bytearray(payload_c)
for p, _ in target_payloads:
    i = 0
    while i < len(p) - 1:
        al = p[i]
        if al == 0 or i + al >= len(p):
            break
        at = p[i + 1]
        if at == 0xFF:
            chunk = p[i : i + 1 + al]
            if len(payload_d) + len(chunk) <= 31:
                payload_d += chunk
        i += al + 1
    break
if len(payload_d) > len(payload_c):
    strategies.append((f"Name + manufacturer data", mac_str, bytes(payload_d)))

# E) Fast Pair discoverable
if has_fastpair and fastpair_prefix:
    payload_e = bytearray([0x02, 0x01, 0x06, 0x02, 0x0A, 0xF6, 0x06, 0x16, 0x2C, 0xFE])
    payload_e += bytes.fromhex(fastpair_prefix)
    strategies.append(
        (f"Fast Pair discoverable (0x{fastpair_prefix.upper()})", mac_str, bytes(payload_e))
    )

# F) Random MAC + name
rmac = bytearray(os.urandom(6))
rmac[5] |= 0xC0
rmac_str = ":".join(f"{b:02X}" for b in rmac)
strategies.append((f"Random MAC + name", rmac_str, bytes(payload_c)))

# G) All payloads alternating
if len(target_payloads) > 1:
    strategies.append(("ALL_ALTERNATING", mac_str, None))

# Run each strategy
for idx, strategy in enumerate(strategies):
    name, smac, payload = strategy

    print(f"\n{'='*60}")
    print(f"Strategy {idx+1}/{len(strategies)}: {name}")
    print(f"MAC: {smac}")
    if payload:
        print(f"Payload ({len(payload)}B): {payload.hex()}")

    if name == "ALL_ALTERNATING":
        # Special: alternate all payloads
        import threading

        stop = threading.Event()
        count = [0]
        radio.set_ble_addr_str(smac)
        radio.set_phy(PHY.BLE_1M, channel=37)

        def alt_loop():
            while not stop.is_set():
                for p, _ in target_payloads:
                    if stop.is_set():
                        break
                    try:
                        radio.transmit(p, power_dbm=5)
                        count[0] += 1
                    except:
                        pass
                    time.sleep(0.02)

        t = threading.Thread(target=alt_loop, daemon=True)
        t.start()
        input("    Transmitting all payloads... Press Enter to stop.")
        stop.set()
        t.join(timeout=2)
        print(f"    Sent {count[0]} pkts")
        time.sleep(0.5)
    else:
        wait("Press Enter to start transmitting (Enter again to stop)...")
        emit_until_enter(radio, smac, payload)

    cont = input("    Continue to next strategy? (y/n): ").strip().lower()
    if cont == "n":
        break

radio.disconnect()
print("\nDone!")
