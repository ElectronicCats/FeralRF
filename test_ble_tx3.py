#!/usr/bin/env python3
"""BLE TX verify: transmit continuously while external scanner checks."""

import sys
import time
sys.path.insert(0, "python")

from feralrf.radio import Radio
from feralrf.enums import PHY

PORT_TX = "/dev/ttyACM0"

# Our TX uses device address AA:BB:CC:DD:EE:01 (random)
TEST_PAYLOAD = bytes([0x02, 0x01, 0x06, 0x05, 0xFF, 0xCA, 0xFE, 0xBE, 0xEF, 0x42])

print("=== BLE TX Continuous Verify ===\n")
print("Expected device address: AA:BB:CC:DD:EE:01")
print("Run in another terminal:")
print("  sudo hcitool lescan")
print("  or: sudo btmon &  then  sudo hcitool lescan")
print()

tx = Radio(port=PORT_TX, baudrate=921600)
tx.connect()
tx.init()
tx.set_phy(PHY.BLE_1M, channel=37)

print("Transmitting BLE ADV_NC on ch37/38/39 every 100ms for 30s...")
print("(ADV_NC cycles through all 3 adv channels per TI stack)")
print("Press Ctrl+C to stop\n")

count = 0
errors = 0
try:
    for i in range(300):
        try:
            tx.transmit(TEST_PAYLOAD, power_dbm=5)
            count += 1
        except Exception as e:
            errors += 1
            if errors <= 3:
                print(f"  TX error #{errors}: {e}")
        time.sleep(0.1)
        if (i + 1) % 50 == 0:
            print(f"  ... {count} sent, {errors} errors")
except KeyboardInterrupt:
    print("\nStopped.")

print(f"\nTotal: {count} sent, {errors} errors")
tx.disconnect()
