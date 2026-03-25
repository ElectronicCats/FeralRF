#!/usr/bin/env python3
"""Quick BLE TX test: B→A only (CC1352P7 → CC1352P)."""
import sys, time, threading
sys.path.insert(0, "python")
from feralrf.radio import Radio
from feralrf.enums import PHY

ADDR = bytes([0x01, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA])
CAFE = bytes([0xCA, 0xFE])
PAYLOAD = bytes([0x02, 0x01, 0x06, 0x05, 0xFF, 0xCA, 0xFE, 0xBE, 0xEF, 0x42])

a = Radio(port="/dev/ttyACM0", baudrate=921600)  # RX (CC1352P)
b = Radio(port="/dev/ttyACM3", baudrate=921600)  # TX (CC1352P7)
a.connect(); b.connect()

b.init(); a.init()
time.sleep(0.3)
b.set_phy(PHY.BLE_1M, channel=37)
a.set_phy(PHY.BLE_1M, channel=37)

a.start_rx()
time.sleep(0.3)
collected = []
def collect(r):
    for p in r.read_packets(timeout=8.0):
        collected.append(p)
t = threading.Thread(target=collect, args=(a,))
t.start()
time.sleep(0.3)

tx_ok = tx_err = 0
for i in range(20):
    try:
        b.transmit(PAYLOAD, power_dbm=0)
        tx_ok += 1
    except Exception as e:
        tx_err += 1
        if tx_err <= 3: print(f"  TX err: {e}")
    time.sleep(0.15)

print(f"TX: {tx_ok}/{tx_ok+tx_err}")
t.join(timeout=10)
a.stop_rx()

addr_m = [p for p in collected if ADDR in p.data]
cafe_m = [p for p in collected if CAFE in p.data]
print(f"RX: {len(collected)} total, addr={len(addr_m)}, cafe={len(cafe_m)}")

if addr_m:
    print("PASS!")
    for p in addr_m[:3]:
        print(f"  ch={p.channel} rssi={p.rssi_dbm}dBm hex={p.data.hex()}")
else:
    print("FAIL - no packets matched")
    # Check stats
    stats_b = b.get_stats()
    print(f"TX stats: rx_ok={stats_b.rx_ok} crc_err={stats_b.rx_crc_err}")
    # Show some received packets
    for p in collected[:5]:
        print(f"  ch={p.channel} rssi={p.rssi_dbm}dBm len={len(p.data)} hex={p.data[:20].hex()}")

a.disconnect(); b.disconnect()
