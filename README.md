# FeralRF

Universal firmware for CatSniffer (CC1352P + RP2040) providing RF capabilities: sniffing, TX/RX, jamming, and spectrum analysis for BLE-PHY, Zigbee, and Sub-1GHz.

## Features

- **Multi-protocol support**: raw BLE-PHY capture (protocol handling via Sniffle), IEEE 802.15.4 (Zigbee), Sub-1GHz (868/915 MHz)
- **Sniffing**: Promiscuous mode packet capture
- **TX/RX**: Raw packet transmission and reception
- **Jamming**: Continuous wave and reactive jamming (<500µs latency)
- **Spectrum analysis**: Frequency scanning and RSSI measurement
- **Python API**: Full-featured Python library for automation

## Hardware

CatSniffer by Electronic Cats:
- CC1352P1F3RGZT (Cortex-M4F + Radio Core)
- RP2040 (USB-CDC Bridge)
- 3Mbps UART with hardware flow control

## Quick Start

### Python API

```bash
pip install feralrf
```

```python
from feralrf import Radio, PHY

with Radio() as radio:
    radio.set_phy(PHY.BLE_1M, channel=37)
    radio.start_rx()

    for packet in radio.read_packets(timeout=10):
        print(f"RSSI: {packet.rssi_dbm} dBm | {packet.data.hex()}")

    radio.stop_rx()
```

### Firmware Build

```bash
# Build Docker container
docker build -t feralrf-build -f docker/Dockerfile .

# Build RP2040 firmware
cd firmware/rp2040 && mkdir build && cd build
cmake .. && make

# Build CC1352 firmware (requires TI SDK)
cd firmware/cc1352 && mkdir build && cd build
cmake .. && make
```

## Examples

Official entrypoints in `python/examples/`:

- `smoke_phase2.py`
- `smoke_phy4_ieee154.py`
- `smoke_prop_phase1.py`
- `smoke_tx_phase1.py`
- `smoke_tx_frame_phase1.py`
- `smoke_tx_burst_phase1.py`
- `smoke_tx_continuous_phase1.py`
- `release_gate_multi_phy.py`
- `run_validation_baseline.sh`

Manual, OTA, soak, demo, and characterization workflows now live under `python/examples/lab/`.

## Documentation

- [Validation Matrix](docs/VALIDATION_MATRIX.md) - Baseline matrix and recommended validation flow
- [Python API](docs/PYTHON_API.md) - Public API status and usage guidance
- [Protocol](docs/protocol.md) - Host/firmware command contract
- [Plan Maestro](docs/PLAN_MAESTRO.md) - Architecture and development phases
- [Hardware Pinout](hardware/PINOUT.md) - CatSniffer pinout reference

## Requirements

### Firmware
- TI SimpleLink CC13xx/CC26xx SDK 7.10.01.24
- ARM GCC toolchain
- Pico SDK 2.0.0
- CMake 3.20+

### Python
- Python 3.9+
- pyserial, pyserial-asyncio

## License

GPL-3.0 - See [LICENSE](LICENSE)

## Warning

⚠️ RF jamming may be illegal in your jurisdiction. Only use in authorized environments for research, security testing, and educational purposes.

---

Made with :heart: by [Electronic Cats](https://electroniccats.com)
