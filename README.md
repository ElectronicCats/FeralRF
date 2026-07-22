# FeralRF

Universal firmware and Python host API for the CatSniffer (CC1352P7 + RP2040):
multi-PHY RF sniffing, raw TX/RX, and on-chip crypto for the CC1352P7 radio,
exposed through a Python API.

The goal is to expose the CC1352P7 radio surface (protocols, modulations and
crypto accelerators the chip supports) cleanly from Python, so you can build
any RF workflow on top.

## Features

Coverage below is scoped to the CC1352P7. Status legend:

- **Stable** - validated over-the-air on this hardware.
- **Experimental** - present but not baseline-validated, or hardware-marginal.
- **Planned** - on the roadmap, not yet available.

### Protocols

| Protocol | Status | Notes |
|---|---|---|
| IEEE 802.15.4 | Stable | 2.4 GHz O-QPSK, channels 11-26 |
| Zigbee / Thread / Matter / 6LoWPAN | Stable | ride on the 802.15.4 PHY (stack crafted in Python) |
| Proprietary Sub-1GHz | Stable | 868 / 915 MHz |
| Wireless M-Bus | Stable | S / T / C modes at 868 MHz |
| Wi-SUN | Experimental | FSK presets; baseline validation pending |
| Amazon Sidewalk | Experimental | Sub-GHz FSK layer only (LR uses LoRa, out of scope) |
| Proprietary 2.4 GHz | Experimental | CW works; modulated GFSK round-trip pending |
| MIOTY | Planned | TS-UNB not natively supported without a custom CPE patch |

### Modulation schemes

| Scheme | Status | Notes |
|---|---|---|
| 2(G)FSK | Stable | multiple bands |
| 4(G)FSK | Stable | 868 validated; 433 experimental |
| (G)MSK | Stable | 868 validated; 433 marginal |
| OOK / ASK | Stable | 868 validated; 433 marginal (antenna-limited) |

### Cryptographic accelerators

| Accelerator | Status | Notes |
|---|---|---|
| AES | Stable | ECB / CTR / CBC / CCM / GCM |
| ECC | Stable | ECDH (P-256, Curve25519), ECDSA (P-256) |
| SHA-2 | Stable | SHA-256 |
| TRNG | Stable | hardware RNG |
| RSA | Planned | not exposed yet |

Plus:

- Raw sniffing and TX/RX on any exposed PHY (`start_rx` / `read_packets` / `transmit*`).
- TX test modes: CW and PRBS, for bench and compliance work.
- KillerBee integration: expose the CatSniffer as a KillerBee IEEE 802.15.4 device.
- Python API (`feralrf`): layered L1-L4 with a stable public `Radio` class.

## Hardware

CatSniffer by Electronic Cats:

- CC1352P7 (Cortex-M4F + radio core) - the RF engine this firmware runs on.
- RP2040 - USB bridge (stock CatSniffer firmware).
- RP2040 <-> CC1352 UART: 921600 baud, hardware flow control OFF.
- Three USB CDC-ACM ports: Cat-Bridge (CC1352 radio link @ 921600), Cat-LoRa, Cat-Shell (config @ 115200).

Full pinout: [hardware/PINOUT.md](hardware/PINOUT.md).

## Getting Started

### 1. Build the CC1352 firmware

The TI SimpleLink SDK is a git submodule.

```bash
git submodule update --init
cd firmware/cc1352 && mkdir -p build && cd build
cmake .. && make -j$(nproc)          # outputs feralrf_cc1352.elf/.hex/.bin
```

Note: the open-source TI SDK ships only the RF-core prebuilt lib. The build also
links three precompiled libs (drivers, driverlib, sysbios) that come from TI's
full installer SDK. Install it and pass
`-DTI_SDK_FULL=~/ti/simplelink_cc13xx_cc26xx_sdk_8_30_01_01`, or copy those three
libs into the submodule.

### 2. Flash it with catnip

Flash the `.hex` (never the `.bin` - it causes boot failures) using catnip, the
Electronic Cats CatSniffer flashing tool
([CatSniffer-Tools](https://github.com/ElectronicCats/CatSniffer-Tools)). catnip
handles bootloader entry, erase, write, verify and reset over the RP2040 shell port.

```bash
catnip devices                                              # list connected CatSniffers
catnip -p /dev/ttyACM0 flash firmware/cc1352/build/feralrf_cc1352.hex
```

### 3. Install the Python package

```bash
cd python
pip install -e ".[dev]"              # add the killerbee extra: pip install -e ".[dev,killerbee]"
```

## Quick Start

```python
from feralrf import PHY, Radio, RxStreamError

radio = Radio()                                  # auto-detects the Cat-Bridge port
radio.init()
radio.set_phy(PHY.IEEE_802_15_4, channel=11)     # Zigbee/Thread, channel 11-26
radio.start_rx()

for pkt in radio.read_packets(timeout=10):
    if isinstance(pkt, RxStreamError):
        continue
    print(f"ch={pkt.channel} rssi={pkt.rssi_dbm} dBm crc_ok={pkt.crc_ok} {pkt.data.hex()}")

radio.stop_rx()
radio.disconnect()
```

## Roadmap

Chip protocols we want to fully expose but that are not ready yet:

- Proprietary 2.4 GHz (complete modulated GFSK)
- Wi-SUN FAN (baseline validation)
- Amazon Sidewalk (baseline validation)
- MIOTY (TS-UNB)

## Examples

- `python/examples/*.py` - smoke and release-gate scripts, run against real hardware.
- `python/examples/lab/*` - manual, OTA, soak, and demo workflows.

## Documentation

- [Architecture](docs/ARCHITECTURE.md) - layered design and RF rules
- [Python API](docs/PYTHON_API.md) - public API status and usage
- [Protocol](docs/protocol.md) - host/firmware wire-format contract
- [Validation Matrix](docs/VALIDATION_MATRIX.md) - validation baseline
- [Testing on Linux](docs/TESTING-ON-LINUX.md) - end-to-end KillerBee runbook
- [Hardware Pinout](hardware/PINOUT.md) - CatSniffer pinout reference

## Requirements

### Firmware

- TI SimpleLink CC13xx/CC26xx SDK 8.30.01.01 (git submodule)
- ARM GCC toolchain
- CMake 3.20+

### Python

- Python 3.9+
- pyserial, pyserial-asyncio
- Optional: `killerbee` >= 3.0.0 (KillerBee integration)

## License

GPL-3.0 - see [LICENSE](LICENSE).

## Warning

RF transmission (including test modes and jamming) may be regulated in your
jurisdiction. Use only in authorized environments for research, security
testing, and educational purposes.

---

Made with :heart: by [Electronic Cats](https://electroniccats.com)
