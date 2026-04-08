"""Test 433 MHz TX — send DEADBEEF markers for board 2 RX verification."""

import sys
import time

from feralrf import PHY, Radio

PORT = "/dev/ttyACM3"
COUNT = int(sys.argv[1]) if len(sys.argv) > 1 else 20
INTERVAL = float(sys.argv[2]) if len(sys.argv) > 2 else 1.0

radio = Radio(PORT)
radio.connect()
time.sleep(0.3)
info = radio.init()
print(f"Connected: {info}")

radio.set_phy(PHY.PROPRIETARY_GFSK, channel=0, frequency_hz=433920000)
time.sleep(0.3)
radio.set_power(12)

# Same 30-byte format as hardcoded boot test
print(f"Sending {COUNT} packets at 433.92 MHz (12 dBm), {INTERVAL}s interval...")
ok = fail = 0
for i in range(COUNT):
    pkt = bytearray(30)
    pkt[0] = (i >> 8) & 0xFF
    pkt[1] = i & 0xFF
    pkt[2] = 0xDE
    pkt[3] = 0xAD
    pkt[4] = 0xBE
    pkt[5] = 0xEF
    for k in range(6, 30):
        pkt[k] = k
    try:
        radio.transmit(bytes(pkt))
        ok += 1
        print(f"  TX {i+1}/{COUNT} OK")
    except Exception as e:
        fail += 1
        print(f"  TX {i+1}/{COUNT} FAIL: {e}")
    time.sleep(INTERVAL)

print(f"Result: {ok}/{COUNT} OK, {fail} FAIL")
radio.disconnect()
