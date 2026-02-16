# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FeralRF is a universal firmware for CatSniffer hardware (CC1352P + RP2040) providing RF capabilities: sniffing, TX/RX, jamming, and spectrum analysis for BLE, Zigbee, and Sub-1GHz (868/915 MHz).

**License:** GPL-3.0

## Architecture

```
HOST (Python API) <-> RP2040 (USB Bridge) <-> CC1352P (Radio Engine)
```

- **CC1352P**: Radio operations, COBS protocol, command processing, TI-RTOS 7
- **RP2040**: USB-CDC bridge, timestamping, CC1352 reset monitoring, flow control
- **Python API**: `feralrf` package, async/sync interfaces, pyserial-asyncio

## Hardware Configuration

See `hardware/PINOUT.md` for full details.

### RP2040 ↔ CC1352 UART (3Mbps)
| Signal | RP2040 | CC1352 |
|--------|--------|--------|
| TX | GPIO0 | DIO12 |
| RX | GPIO1 | DIO13 |
| RTS | GPIO2 | DIO14 |
| CTS | GPIO3 | DIO15 |

### Control & LEDs
| Signal | RP2040 GPIO |
|--------|-------------|
| RESET_CC | GPIO15 |
| LED1 | GPIO28 |
| LED2 | GPIO27 |
| LED3 | GPIO26 |

### Notes
- SWD not available (JTAG only on DIO16/DIO17)
- Bootloader via UART (TI ROM BSL)
- LEDs are active low

## Build Commands

```bash
# Build Docker container
docker build -t feralrf-build -f docker/Dockerfile .

# Build CC1352 firmware (requires TI SDK in firmware/sdk/)
cd firmware/cc1352 && mkdir -p build && cd build
cmake .. && make -j$(nproc)

# Build RP2040 firmware (Pico SDK auto-downloaded)
cd firmware/rp2040 && mkdir -p build && cd build
cmake .. -DPICO_SDK_PATH=../../sdk/pico-sdk && make -j$(nproc)

# Build Python package
cd python && pip install -e ".[dev]"
```

## Test Commands

```bash
# Python unit tests
cd python && source .venv/bin/activate && pytest

# Python tests with coverage
pytest --cov=feralrf

# Hardware integration tests
pytest -m hardware
```

## Current Status (FASE 0)

| Component | Status |
|-----------|--------|
| RP2040 firmware | ✅ Compiles (feralrf_rp2040.uf2) |
| CC1352 firmware | ⏳ In progress |
| Python package | ✅ Tests passing (13/13) |
| GitHub Actions | ✅ Configured |

## Protocol

COBS-framed binary protocol with CRC16-CCITT. See `PLAN_MAESTRO.md` section 3 for full command IDs and frame format.

Key response codes: `RSP_ACK (0x80)`, `RSP_ERROR (0x81)`, `RSP_RX_PACKET (0x90)`

## Python Dependencies

| Package | Purpose |
|---------|---------|
| pyserial | Serial communication |
| pyserial-asyncio | Async serial support |
| cobs | COBS encoding/decoding |

## Development Phases

See `PLAN_MAESTRO.md` section 5. Current phase tracking maintained there.

- Phase 0: Setup (Docker, CMake, skeleton)
- Phase 1: BLE Sniffer MVP
- Phase 2: TX + Basic Jamming
- Phase 3: Spectrum Analyzer
- Phase 4: Zigbee + Multi-PHY
- Phase 5: Reactive Jamming (<500µs target)
- Phase 6: Sub-1GHz + Testing
- Phase 7: Bootloader + Release

## Key Constraints

- **Memory**: Static allocation only (no malloc) on CC1352
- **RX Buffer**: 16KB circular buffer
- **TX Power**: -20 to +20 dBm
- **Reactive Jamming**: <500µs latency requirement
- **SDK Version**: TI SimpleLink CC13xx/CC26xx SDK 7.10.01.24 (fixed)
