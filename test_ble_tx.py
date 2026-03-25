#!/usr/bin/env python3
"""BLE TX validation: TX on fixed channel, RX on same channel, look for known device address."""

import sys
import time
import threading
sys.path.insert(0, "python")

from feralrf.radio import Radio
from feralrf.enums import PHY

PORT_TX = "/dev/ttyACM0"  # CatSniffer #1 — transmitter
PORT_RX = "/dev/ttyACM3"  # CatSniffer #2 — receiver

# The firmware uses this device address for ADV_NC:
# {0x01, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA} (little-endian in memory)
# Over the air (BLE is little-endian): 01:EE:DD:CC:BB:AA
KNOWN_ADDR = bytes([0x01, 0xEE, 0xDD, 0xCC, 0xBB, 0xAA])

TEST_PAYLOAD = bytes([0x02, 0x01, 0x06, 0x05, 0xFF, 0xCA, 0xFE, 0xBE, 0xEF, 0x42])
CHANNEL = 37

collected = []
rx_done = threading.Event()

def rx_collector(radio, duration):
    try:
        for pkt in radio.read_packets(timeout=duration):
            collected.append(pkt)
    except Exception as e:
        print(f"  RX error: {e}")
    rx_done.set()


print("=== BLE TX Validation ===\n")

tx = Radio(port=PORT_TX, baudrate=921600)
rx = Radio(port=PORT_RX, baudrate=921600)

print("[*] Connecting...")
tx.connect()
rx.connect()

# Init both
tx.init()
rx.init()

# Set both to BLE 1M, channel 37 (fixed, no hopping)
tx.set_phy(PHY.BLE_1M, channel=CHANNEL)
rx.set_phy(PHY.BLE_1M, channel=CHANNEL)

# Start RX
print(f"[*] RX listening on ch{CHANNEL}...")
rx.start_rx()

# Start collection thread
t = threading.Thread(target=rx_collector, args=(rx, 6.0))
t.start()
time.sleep(0.5)

# Transmit
print(f"[*] TX sending 20 packets on ch{CHANNEL}...")
tx_ok = 0
tx_fail = 0
for i in range(20):
    try:
        tx.transmit(TEST_PAYLOAD, power_dbm=5)
        tx_ok += 1
    except Exception as e:
        tx_fail += 1
        if tx_fail <= 2:
            print(f"  TX #{i} error: {e}")
    time.sleep(0.15)

print(f"[*] TX done: {tx_ok} ok, {tx_fail} fail")

# Wait for RX to finish
print("[*] Waiting for RX collection...")
rx_done.wait(timeout=10)
rx.stop_rx()

# Analyze
print(f"\n[*] Total packets received: {len(collected)}")

our_pkts = []
other_pkts = []
for pkt in collected:
    # ADV_NC PDU: [header(2)] [AdvA(6)] [AdvData(...)]
    # header byte 0 lower nibble = PDU type: ADV_NONCONN_IND = 0x02
    if len(pkt.data) >= 8 and KNOWN_ADDR in pkt.data:
        our_pkts.append(pkt)
    else:
        other_pkts.append(pkt)

print(f"[*] Our TX packets (addr AA:BB:CC:DD:EE:01): {len(our_pkts)}")
print(f"[*] Other ambient packets: {len(other_pkts)}")

if our_pkts:
    print("\n  === OUR PACKETS ===")
    for i, pkt in enumerate(our_pkts[:5]):
        print(f"  #{i+1} ch={pkt.channel} rssi={pkt.rssi_dbm}dBm len={len(pkt.data)}")
        print(f"       raw: {pkt.data.hex()}")
        # Parse: header(2) + addr(6) + advdata
        if len(pkt.data) >= 8:
            hdr = pkt.data[0:2].hex()
            addr = pkt.data[2:8].hex()
            advdata = pkt.data[8:].hex()
            print(f"       hdr={hdr} addr={addr} advdata={advdata}")

    # Check if our payload is in the advdata
    payload_hex = TEST_PAYLOAD.hex()
    matched = sum(1 for p in our_pkts if TEST_PAYLOAD in p.data)
    print(f"\n  Payload match ({payload_hex}): {matched}/{len(our_pkts)}")
    print(f"\n  Result: {'PASS' if matched > 0 else 'PARTIAL - addr match but payload mismatch'}")
else:
    print("\n  Result: FAIL — none of our TX packets were received")
    if other_pkts:
        print("  (RX is working — got ambient BLE, but our TX not seen)")
        print("  Sample ambient:")
        for p in other_pkts[:3]:
            print(f"    ch={p.channel} rssi={p.rssi_dbm} data={p.data[:16].hex()}")

tx.disconnect()
rx.disconnect()
print("\n=== Done ===")
