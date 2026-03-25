#!/usr/bin/env python3
"""BLE TX debug: understand exact packet structure seen by RX."""

import sys
import time
import threading
sys.path.insert(0, "python")

from feralrf.radio import Radio
from feralrf.enums import PHY

PORT_TX = "/dev/ttyACM0"
PORT_RX = "/dev/ttyACM3"

# TX device addr in firmware: {0x01, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA}
# BLE ADV PDU over air: [Header(2)][AdvA(6)][AdvData(n)]
# AdvA is transmitted LSB first, so over the air: 01 EE DD CC BB AA
# But RX reports it as received bytes, so we search both orderings
ADDR_LE = bytes([0x01, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA])
ADDR_BE = bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0x01])
CAFE = bytes([0xCA, 0xFE])

TEST_PAYLOAD = bytes([0x02, 0x01, 0x06, 0x05, 0xFF, 0xCA, 0xFE, 0xBE, 0xEF, 0x42])

# Step 1: Capture baseline (no TX) for 3 seconds
# Step 2: TX while capturing for 5 seconds
# Step 3: Compare — any NEW unique packets are our TX

print("=== BLE TX Structure Debug ===\n")

tx = Radio(port=PORT_TX, baudrate=921600)
rx = Radio(port=PORT_RX, baudrate=921600)
tx.connect()
rx.connect()
time.sleep(0.5)
tx.init()
time.sleep(0.3)
rx.init()

# Fixed channel 37 for BOTH
tx.set_phy(PHY.BLE_1M, channel=37)
rx.set_phy(PHY.BLE_1M, channel=37)

# Step 1: Baseline
print("[1] Capturing baseline (3s, no TX)...")
rx.start_rx()
baseline = set()
t0 = time.time()
for pkt in rx.read_packets(timeout=3.0):
    # Use first 8 bytes as signature (header + addr)
    sig = pkt.data[:8]
    baseline.add(sig)
rx.stop_rx()
rx.init()
rx.set_phy(PHY.BLE_1M, channel=37)
print(f"   Baseline: {len(baseline)} unique packet signatures\n")

# Step 2: TX + RX
print("[2] TX + RX simultaneously (5s)...")
rx.start_rx()

collected = []
rx_done = threading.Event()
def collect(radio):
    for pkt in radio.read_packets(timeout=6.0):
        collected.append(pkt)
    rx_done.set()

t = threading.Thread(target=collect, args=(rx,))
t.start()
time.sleep(0.3)

# Transmit
tx_count = 0
for i in range(50):
    try:
        tx.transmit(TEST_PAYLOAD, power_dbm=5)
        tx_count += 1
    except Exception as e:
        print(f"   TX error: {e}")
        break
    time.sleep(0.1)

print(f"   TX sent: {tx_count}")
rx_done.wait(timeout=8)
rx.stop_rx()

# Step 3: Analyze
print(f"   RX total: {len(collected)} packets\n")

# Find new packets not in baseline
new_pkts = []
for pkt in collected:
    sig = pkt.data[:8]
    if sig not in baseline:
        new_pkts.append(pkt)

print(f"[3] NEW packets (not in baseline): {len(new_pkts)}")
if new_pkts:
    print("\n   New packet details:")
    for i, p in enumerate(new_pkts[:10]):
        has_addr = ADDR_LE in p.data or ADDR_BE in p.data
        has_cafe = CAFE in p.data
        print(f"   #{i+1} ch={p.channel} rssi={p.rssi_dbm}dBm len={len(p.data)} addr={'YES' if has_addr else 'no'} cafe={'YES' if has_cafe else 'no'}")
        print(f"        hex: {p.data.hex()}")
else:
    print("   => NO new packets. TX is NOT reaching RX over the air.")
    print("\n   All collected sample:")
    for i, p in enumerate(collected[:5]):
        print(f"   #{i+1} ch={p.channel} rssi={p.rssi_dbm} len={len(p.data)} hex={p.data[:16].hex()}")

# Also search ALL collected for our markers
print(f"\n[4] Searching ALL {len(collected)} packets for markers...")
addr_match = [p for p in collected if ADDR_LE in p.data or ADDR_BE in p.data]
cafe_match = [p for p in collected if CAFE in p.data]
print(f"   Addr match: {len(addr_match)}")
print(f"   CAFE match: {len(cafe_match)}")
if cafe_match:
    for p in cafe_match[:3]:
        print(f"     hex: {p.data.hex()}")

tx.disconnect()
rx.disconnect()
print("\n=== Done ===")
