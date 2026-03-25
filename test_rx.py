#!/usr/bin/env python3
"""RX validation for all supported PHYs."""

import sys
import time
sys.path.insert(0, "python")

from feralrf.radio import Radio
from feralrf.enums import PHY

PORT = "/dev/ttyACM0"

TESTS = [
    {"phy": PHY.BLE_1M,         "channel": 37, "label": "BLE 1M (ch37 adv)",    "duration": 5},
    {"phy": PHY.BLE_2M,         "channel": 37, "label": "BLE 2M (ch37 adv)",    "duration": 5},
    {"phy": PHY.IEEE_802_15_4,  "channel": 15, "label": "IEEE 802.15.4 (ch15)", "duration": 5},
    {"phy": PHY.IEEE_802_15_4,  "channel": 25, "label": "IEEE 802.15.4 (ch25)", "duration": 5},
]

radio = Radio(port=PORT, baudrate=921600)

print("=== FeralRF RX Validation ===\n")
print("[*] Connecting...")
radio.connect()

info = radio.init()
print(f"[*] FW: {info.firmware_version}  Serial: {info.serial}\n")

for i, test in enumerate(TESTS, 1):
    phy = test["phy"]
    ch = test["channel"]
    label = test["label"]
    dur = test["duration"]

    print(f"--- Test {i}/{len(TESTS)}: {label} ---")

    # Set PHY
    try:
        radio.set_phy(phy, channel=ch)
    except Exception as e:
        print(f"  SET_PHY FAIL: {e}\n")
        continue

    # Start RX
    try:
        radio.start_rx()
    except Exception as e:
        print(f"  RX_START FAIL: {e}\n")
        continue

    # Collect packets
    count = 0
    t0 = time.time()
    try:
        for pkt in radio.read_packets(timeout=float(dur)):
            count += 1
            data_hex = pkt.data[:12].hex()
            if count <= 3:
                print(f"  pkt#{count} ch={pkt.channel} rssi={pkt.rssi_dbm}dBm len={len(pkt.data)} {data_hex}")
            if count >= 50:
                break
    except KeyboardInterrupt:
        pass
    except Exception as e:
        print(f"  RX error: {e}")

    elapsed = time.time() - t0

    # Stats
    try:
        stats = radio.get_stats()
        print(f"  => {count} pkts in {elapsed:.1f}s | rx_ok={stats.rx_ok} crc_err={stats.rx_crc_err} drop={stats.rx_drop} overflow={stats.rx_overflow}")
    except Exception as e:
        print(f"  => {count} pkts in {elapsed:.1f}s | stats error: {e}")

    # Stop RX
    try:
        radio.stop_rx()
    except Exception:
        pass

    # Re-init between PHY switches to clean state
    try:
        radio.init()
    except Exception:
        pass

    result = "PASS (packets received)" if count > 0 else "NO PACKETS (may be normal if no devices nearby)"
    print(f"  Result: {result}\n")

radio.disconnect()
print("=== Done ===")
