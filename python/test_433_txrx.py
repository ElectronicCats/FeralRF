"""Test 433 MHz TX/RX between two CatSniffer boards."""

import threading
import time

from feralrf import PHY, Radio

TX_PORT = "/dev/ttyACM3"  # Board 1
RX_PORT = "/dev/ttyACM0"  # Board 2
COUNT = 20
INTERVAL = 1.0

rx_packets = []


def rx_thread():
    radio = Radio(RX_PORT)
    radio.connect()
    time.sleep(0.3)
    radio.init()
    radio.set_phy(PHY.PROPRIETARY_GFSK, channel=0, frequency_hz=433920000)
    time.sleep(0.3)
    radio.start_rx()
    print(f"[RX] Listening on {RX_PORT} at 433.92 MHz...")

    start = time.time()
    while time.time() - start < (COUNT * INTERVAL) + 5:
        pkts = radio.read_packets(timeout=1.0)
        for p in pkts:
            rx_packets.append(p)
            print(f"  [RX] #{len(rx_packets)}: {len(p.data)}B data={p.data[:8].hex()}")

    radio.stop_rx()
    radio.disconnect()


# Start RX in background
t = threading.Thread(target=rx_thread, daemon=True)
t.start()
time.sleep(2)  # Let RX settle

# TX
tx = Radio(TX_PORT)
tx.connect()
time.sleep(0.3)
tx.init()
tx.set_phy(PHY.PROPRIETARY_GFSK, channel=0, frequency_hz=433920000)
time.sleep(0.3)
tx.set_power(12)

print(f"[TX] Sending {COUNT} packets on {TX_PORT} at 433.92 MHz...")
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
        tx.transmit(bytes(pkt))
        ok += 1
    except Exception as e:
        fail += 1
        print(f"  [TX] FAIL: {e}")
    time.sleep(INTERVAL)

print(f"[TX] Done: {ok}/{COUNT} sent")
tx.disconnect()

# Wait for RX to finish
t.join(timeout=10)
print(f"\n=== RESULT: TX {ok}/{COUNT}, RX {len(rx_packets)} received ===")
