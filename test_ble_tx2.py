#!/usr/bin/env python3
"""BLE TX debug: TX from #1, RX on #2 — dump ALL packets, disable hopping."""

import sys
import time
import threading
sys.path.insert(0, "python")

from feralrf.radio import Radio
from feralrf.enums import PHY

PORT_TX = "/dev/ttyACM0"
PORT_RX = "/dev/ttyACM3"
CHANNEL = 39  # Use ch39, less noisy than 37

# Known TX device addr: 01:EE:DD:CC:BB:AA (as bytes in packet)
KNOWN_ADDR = bytes([0x01, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA])
TEST_PAYLOAD = bytes([0x02, 0x01, 0x06, 0x05, 0xFF, 0xCA, 0xFE, 0xBE, 0xEF, 0x42])

collected = []
rx_stop = threading.Event()

def rx_collector(radio):
    try:
        for pkt in radio.read_packets(timeout=10.0):
            collected.append(pkt)
            if rx_stop.is_set():
                break
    except Exception as e:
        print(f"  RX thread error: {e}")

print("=== BLE TX Debug Test ===\n")

tx = Radio(port=PORT_TX, baudrate=921600)
rx = Radio(port=PORT_RX, baudrate=921600)

tx.connect()
rx.connect()

tx.init()
rx.init()

# Both on same fixed channel — use set_channel AFTER set_phy to override hop
tx.set_phy(PHY.BLE_1M, channel=CHANNEL)
rx.set_phy(PHY.BLE_1M, channel=CHANNEL)

# Start RX
rx.start_rx()
time.sleep(0.3)

# Start RX collection
t = threading.Thread(target=rx_collector, args=(rx,))
t.start()
time.sleep(0.5)

# Send 30 packets with 200ms spacing (slower = more time on air)
print(f"[TX] Sending 30 BLE ADV_NC on ch{CHANNEL}, power=0dBm...")
for i in range(30):
    try:
        tx.transmit(TEST_PAYLOAD, power_dbm=0)
    except Exception as e:
        print(f"  TX #{i} error: {e}")
        break
    time.sleep(0.2)

print("[TX] Done. Waiting for RX to finish...")
time.sleep(2)
rx_stop.set()
t.join(timeout=5)
rx.stop_rx()

# Analyze ALL packets
print(f"\n[RX] Total: {len(collected)} packets on ch{CHANNEL}")

# Group by likely source
our = []
ambient = []
for pkt in collected:
    if KNOWN_ADDR in pkt.data:
        our.append(pkt)
    else:
        ambient.append(pkt)

print(f"[RX] Our (addr match): {len(our)}")
print(f"[RX] Ambient: {len(ambient)}")

if our:
    print("\n  Our TX packets:")
    for i, p in enumerate(our[:5]):
        print(f"    #{i+1} ch={p.channel} rssi={p.rssi_dbm}dBm data={p.data.hex()}")
else:
    print("\n  No packets with our device address found.")
    print("  Checking if ANY packet contains 0xCAFE from payload:")
    cafe = [p for p in collected if b'\xCA\xFE' in p.data]
    print(f"  Packets with CAFE: {len(cafe)}")

    if ambient:
        print(f"\n  Ambient sample (first 5):")
        for i, p in enumerate(ambient[:5]):
            # Show full hex to see if there's anything recognizable
            print(f"    #{i+1} ch={p.channel} rssi={p.rssi_dbm}dBm len={len(p.data)} data={p.data.hex()}")

# Get stats
stats_tx = tx.get_stats()
stats_rx = rx.get_stats()
print(f"\n[TX stats] rx_ok={stats_tx.rx_ok} crc_err={stats_tx.rx_crc_err}")
print(f"[RX stats] rx_ok={stats_rx.rx_ok} crc_err={stats_rx.rx_crc_err} drop={stats_rx.rx_drop}")

tx.disconnect()
rx.disconnect()
print("\n=== Done ===")
