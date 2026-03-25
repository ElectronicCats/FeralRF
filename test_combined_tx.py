#!/usr/bin/env python3
"""Combined TX test: 802.15.4 (known working) then BLE (broken)."""

import sys
import time
import threading
sys.path.insert(0, "python")

from feralrf.radio import Radio
from feralrf.enums import PHY

PORT_TX = "/dev/ttyACM0"
PORT_RX = "/dev/ttyACM3"

KNOWN_ADDR = bytes([0x01, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA])
CAFE = bytes([0xCA, 0xFE])
TEST_PAYLOAD = bytes([0x02, 0x01, 0x06, 0x05, 0xFF, 0xCA, 0xFE, 0xBE, 0xEF, 0x42])
IEEE_PAYLOAD = bytes([0xCA, 0xFE, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04])

def collect_packets(radio, timeout, result_list):
    for pkt in radio.read_packets(timeout=timeout):
        result_list.append(pkt)

tx = Radio(port=PORT_TX, baudrate=921600)
rx = Radio(port=PORT_RX, baudrate=921600)
tx.connect()
rx.connect()

# ============ TEST 1: IEEE 802.15.4 TX ============
print("=== TEST 1: IEEE 802.15.4 TX ===")
tx.init()
rx.init()
time.sleep(0.3)
tx.set_phy(PHY.IEEE_802_15_4, channel=25)
rx.set_phy(PHY.IEEE_802_15_4, channel=25)

rx.start_rx()
time.sleep(0.3)
collected = []
t = threading.Thread(target=collect_packets, args=(rx, 5.0, collected))
t.start()
time.sleep(0.3)

tx_ok = 0
for i in range(10):
    try:
        tx.transmit(IEEE_PAYLOAD, power_dbm=0)
        tx_ok += 1
    except Exception as e:
        print(f"  TX error: {e}")
    time.sleep(0.15)

print(f"  TX sent: {tx_ok}")
t.join(timeout=8)
rx.stop_rx()

cafe_match = [p for p in collected if CAFE in p.data]
print(f"  RX total: {len(collected)}, CAFE match: {len(cafe_match)}")
if cafe_match:
    for p in cafe_match[:3]:
        print(f"    ch={p.channel} rssi={p.rssi_dbm}dBm hex={p.data.hex()}")
print(f"  802.15.4 TX: {'PASS' if cafe_match else 'FAIL'}\n")

# ============ TEST 2: BLE TX ============
print("=== TEST 2: BLE TX ===")
tx.init()
rx.init()
time.sleep(0.3)
tx.set_phy(PHY.BLE_1M, channel=37)
rx.set_phy(PHY.BLE_1M, channel=37)

# Capture baseline (ambient BLE)
print("  Baseline (2s)...")
rx.start_rx()
baseline_sigs = set()
for pkt in rx.read_packets(timeout=2.0):
    baseline_sigs.add(pkt.data[:10])
rx.stop_rx()
print(f"    {len(baseline_sigs)} unique sigs")

# TX + RX
tx.init()
rx.init()
time.sleep(0.3)
tx.set_phy(PHY.BLE_1M, channel=37)
rx.set_phy(PHY.BLE_1M, channel=37)

rx.start_rx()
time.sleep(0.3)
collected = []
t = threading.Thread(target=collect_packets, args=(rx, 8.0, collected))
t.start()
time.sleep(0.3)

tx_ok = 0
tx_err = 0
for i in range(20):
    try:
        tx.transmit(TEST_PAYLOAD, power_dbm=0)
        tx_ok += 1
    except Exception as e:
        tx_err += 1
        if tx_err <= 3:
            print(f"  TX error: {e}")
    time.sleep(0.2)

print(f"  TX sent: {tx_ok}, errors: {tx_err}")
time.sleep(2)
t.join(timeout=10)
rx.stop_rx()

# Analyze
new_pkts = [p for p in collected if p.data[:10] not in baseline_sigs]
addr_match = [p for p in collected if KNOWN_ADDR in p.data]
cafe_match = [p for p in collected if CAFE in p.data]

print(f"  RX total: {len(collected)}, new: {len(new_pkts)}")
print(f"  Addr match: {len(addr_match)}, CAFE match: {len(cafe_match)}")

if new_pkts:
    print("  New packets:")
    for i, p in enumerate(new_pkts[:5]):
        print(f"    #{i+1} ch={p.channel} rssi={p.rssi_dbm}dBm len={len(p.data)} hex={p.data.hex()}")

if not addr_match and not cafe_match:
    print("  BLE TX: FAIL - no packets received")
    # Dump all collected for analysis
    print(f"  All {len(collected)} packets:")
    for i, p in enumerate(collected[:8]):
        print(f"    #{i+1} ch={p.channel} rssi={p.rssi_dbm}dBm len={len(p.data)} hex={p.data[:20].hex()}")
else:
    print(f"  BLE TX: PASS")

tx.disconnect()
rx.disconnect()
print("\n=== Done ===")
