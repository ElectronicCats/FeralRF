#!/usr/bin/env python3
"""TX/RX validation: CatSniffer #1 TX → CatSniffer #2 RX (and vice versa)."""

import sys
import time
import threading
sys.path.insert(0, "python")

from feralrf.radio import Radio
from feralrf.enums import PHY

PORT_A = "/dev/ttyACM0"  # CatSniffer #1 (CC1352P)
PORT_B = "/dev/ttyACM3"  # CatSniffer #2 (CC1352P7)

# Known test payload
TEST_PAYLOAD_BLE = bytes([0x02, 0x01, 0x06, 0x03, 0xFF, 0xCA, 0xFE])
TEST_PAYLOAD_154 = bytes([0x41, 0x88, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xCA, 0xFE, 0xBA, 0xBE])

def rx_thread_func(radio, results, duration=5.0):
    """Collect packets in background."""
    pkts = []
    try:
        for pkt in radio.read_packets(timeout=duration):
            pkts.append(pkt)
    except Exception as e:
        results["error"] = str(e)
    results["packets"] = pkts


def test_tx_rx(tx_radio, rx_radio, phy, channel, payload, label):
    print(f"\n--- {label} ---")

    # Init both
    for r, name in [(tx_radio, "TX"), (rx_radio, "RX")]:
        try:
            r.init()
            r.set_phy(phy, channel=channel)
        except Exception as e:
            print(f"  {name} init FAIL: {e}")
            return False

    # Start RX on receiver
    try:
        rx_radio.start_rx()
    except Exception as e:
        print(f"  RX start FAIL: {e}")
        return False

    # Start RX collection in background
    results = {}
    rx_t = threading.Thread(target=rx_thread_func, args=(rx_radio, results, 5.0))
    rx_t.start()

    # Give RX a moment to start
    time.sleep(0.5)

    # Transmit several packets
    tx_ok = 0
    tx_fail = 0
    for i in range(10):
        try:
            tx_radio.transmit(payload, power_dbm=0)
            tx_ok += 1
        except Exception as e:
            tx_fail += 1
            if i == 0:
                print(f"  TX error: {e}")
        time.sleep(0.1)

    print(f"  TX: {tx_ok} sent, {tx_fail} failed")

    # Wait for RX thread
    rx_t.join(timeout=8.0)

    # Stop RX
    try:
        rx_radio.stop_rx()
    except Exception:
        pass

    pkts = results.get("packets", [])
    err = results.get("error")
    if err:
        print(f"  RX error: {err}")

    # Check if any received packet contains our test payload
    matched = 0
    for pkt in pkts:
        if payload[3:] in pkt.data or b'\xCA\xFE' in pkt.data:
            matched += 1

    print(f"  RX: {len(pkts)} total packets, {matched} matched test payload")

    if matched > 0:
        print(f"  Result: PASS")
        return True
    elif len(pkts) > 0:
        print(f"  Result: PARTIAL — got packets but no payload match")
        for p in pkts[:3]:
            print(f"    ch={p.channel} rssi={p.rssi_dbm} data={p.data[:16].hex()}")
        return True
    else:
        print(f"  Result: FAIL — no packets received")
        return False


print("=== FeralRF TX/RX Validation ===\n")

radio_a = Radio(port=PORT_A, baudrate=921600)
radio_b = Radio(port=PORT_B, baudrate=921600)

print("[*] Connecting both radios...")
radio_a.connect()
radio_b.connect()

info_a = radio_a.init()
info_b = radio_b.init()
print(f"  #1 ({PORT_A}): FW {info_a.firmware_version}")
print(f"  #2 ({PORT_B}): FW {info_b.firmware_version}")

# Test 1: BLE TX(A) → RX(B)
test_tx_rx(radio_a, radio_b, PHY.BLE_1M, 37, TEST_PAYLOAD_BLE, "BLE 1M: #1 TX → #2 RX")

# Test 2: BLE TX(B) → RX(A)
test_tx_rx(radio_b, radio_a, PHY.BLE_1M, 37, TEST_PAYLOAD_BLE, "BLE 1M: #2 TX → #1 RX")

# Test 3: 802.15.4 TX(A) → RX(B)
test_tx_rx(radio_a, radio_b, PHY.IEEE_802_15_4, 20, TEST_PAYLOAD_154, "802.15.4: #1 TX → #2 RX (ch20)")

# Test 4: 802.15.4 TX(B) → RX(A)
test_tx_rx(radio_b, radio_a, PHY.IEEE_802_15_4, 20, TEST_PAYLOAD_154, "802.15.4: #2 TX → #1 RX (ch20)")

radio_a.disconnect()
radio_b.disconnect()

print("\n=== Done ===")
