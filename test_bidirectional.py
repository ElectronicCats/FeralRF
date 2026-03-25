#!/usr/bin/env python3
"""Bidirectional TX/RX validation: both 802.15.4 and BLE, both directions."""

import sys
import time
import threading
sys.path.insert(0, "python")

from feralrf.radio import Radio
from feralrf.enums import PHY

PORT_A = "/dev/ttyACM0"  # CatSniffer #1 (CC1352P)
PORT_B = "/dev/ttyACM3"  # CatSniffer #2 (CC1352P7)

CAFE = bytes([0xCA, 0xFE])
ADDR = bytes([0x01, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA])
BLE_PAYLOAD = bytes([0x02, 0x01, 0x06, 0x05, 0xFF, 0xCA, 0xFE, 0xBE, 0xEF, 0x42])
IEEE_PAYLOAD = bytes([0xCA, 0xFE, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04])

def collect_packets(radio, timeout, result_list):
    for pkt in radio.read_packets(timeout=timeout):
        result_list.append(pkt)

def tx_test(tx_radio, rx_radio, phy, payload, marker, n_pkts=10, label=""):
    tx_radio.init()
    rx_radio.init()
    time.sleep(0.3)

    if phy == PHY.BLE_1M:
        tx_radio.set_phy(phy, channel=37)
        rx_radio.set_phy(phy, channel=37)
    else:
        tx_radio.set_phy(phy, channel=25)
        rx_radio.set_phy(phy, channel=25)

    rx_radio.start_rx()
    time.sleep(0.3)
    collected = []
    t = threading.Thread(target=collect_packets, args=(rx_radio, 6.0, collected))
    t.start()
    time.sleep(0.3)

    tx_ok = 0
    for i in range(n_pkts):
        try:
            tx_radio.transmit(payload, power_dbm=0)
            tx_ok += 1
        except Exception as e:
            print(f"    TX error: {e}")
            break
        time.sleep(0.15)

    t.join(timeout=8)
    rx_radio.stop_rx()

    matched = [p for p in collected if marker in p.data]
    status = "PASS" if matched else "FAIL"
    print(f"  {label}: TX={tx_ok}/{n_pkts}, RX total={len(collected)}, matched={len(matched)} [{status}]")
    if matched:
        p = matched[0]
        print(f"    sample: ch={p.channel} rssi={p.rssi_dbm}dBm hex={p.data.hex()}")
    return len(matched) > 0

a = Radio(port=PORT_A, baudrate=921600)
b = Radio(port=PORT_B, baudrate=921600)
a.connect()
b.connect()

results = {}
print("=== Bidirectional TX/RX Validation ===\n")

# 802.15.4: A→B
print("[1] IEEE 802.15.4")
results["ieee_a2b"] = tx_test(a, b, PHY.IEEE_802_15_4, IEEE_PAYLOAD, CAFE, label="A→B")
results["ieee_b2a"] = tx_test(b, a, PHY.IEEE_802_15_4, IEEE_PAYLOAD, CAFE, label="B→A")

# BLE: A→B
print("\n[2] BLE 1M")
results["ble_a2b"] = tx_test(a, b, PHY.BLE_1M, BLE_PAYLOAD, ADDR, label="A→B")
results["ble_b2a"] = tx_test(b, a, PHY.BLE_1M, BLE_PAYLOAD, ADDR, label="B→A")

a.disconnect()
b.disconnect()

print("\n=== Summary ===")
all_pass = True
for k, v in results.items():
    status = "PASS" if v else "FAIL"
    if not v:
        all_pass = False
    print(f"  {k}: {status}")
print(f"\nOverall: {'ALL PASS' if all_pass else 'SOME FAILED'}")
