# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FeralRF is universal firmware plus a Python host API for the CatSniffer's CC1352P7 radio. It exposes the chip's RF surface - sniffing, TX/RX, TX test modes, and on-chip crypto - across IEEE 802.15.4 (Zigbee/Thread) and Sub-1GHz (868/915 MHz, including Wireless M-Bus), plus a KillerBee integration. Raw BLE PHY capture still exists in firmware but is de-emphasized and not a headline feature (BLE protocol/connection handling is out of scope - Sniffle covers that). LoRa/SX1262 and the RP2040 USB bridge are provided by the stock CatSniffer firmware and are not maintained in this repo.

License: GPL-3.0. Authoritative design doc: `docs/ARCHITECTURE.md` (read it before touching firmware or the wire protocol).

## The two build targets

```
HOST (Python: feralrf)  <->  RP2040 USB bridge (stock CatSniffer firmware)  <->  CC1352P7 (TI-RTOS 7 radio engine)
```

Each target has its own toolchain and lives in its own tree. They rarely change together.

- `python/feralrf/` - host API, the layer you will edit most. Pure Python, unit-testable without hardware.
- `firmware/cc1352/` - the radio engine. C, TI-RTOS 7, TI SimpleLink SDK 8.30.01.01.

The RP2040 USB bridge runs the stock CatSniffer firmware (transparent Cat-Bridge passthrough); it is not built or maintained in this repo.

## Build Commands

### CC1352 firmware (`firmware/cc1352/`)

The TI SDK is a git submodule at `firmware/sdk/simplelink_cc13xx_cc26xx_sdk_8_30_01_01` (`git submodule update --init`). Default device variant is `CC1352P7`; `CC1352P` is also selectable via `-DDEVICE_VARIANT=`.

```bash
cd firmware/cc1352 && mkdir -p build && cd build
cmake .. && make -j$(nproc)   # outputs feralrf_cc1352.elf/.hex/.bin
```

Important build caveat: the open-source GitHub SDK ships only the RF core prebuilt lib. The build also links three precompiled libs (drivers, driverlib, sysbios) that come only from TI's full installer SDK. Either install that and point `-DTI_SDK_FULL=~/ti/simplelink_cc13xx_cc26xx_sdk_8_30_01_01`, or copy the three `.a`/`.lib` files into the submodule. Do NOT hardcode a machine-specific `$HOME` path in CMakeLists. The Docker image (`docker build -t feralrf-build -f docker/Dockerfile .`) provides the ARM GCC toolchain.

Flash the `.hex` (via catnip/UART BSL). The `.bin` causes boot failures.

### Python package (`python/`)

```bash
cd python && pip install -e ".[dev]"          # add ",killerbee" for the KillerBee integration
```

## Test Commands

```bash
cd python && pytest                 # unit tests, no hardware (asyncio_mode=auto, testpaths=tests)
pytest --cov=feralrf
pytest tests/test_protocol.py -k crc16    # single file / single test
pytest -m hardware                  # requires a connected FeralRF device
```

Beyond unit tests: `python/examples/*.py` are official smoke/release-gate scripts run against real hardware; `python/examples/lab/*` are manual/OTA/soak/demo workflows. The end-to-end KillerBee-on-Linux runbook (software tests -> firmware -> KillerBee CLI -> key-capture attack) is `docs/TESTING-ON-LINUX.md`.

## Python API structure (`python/feralrf/`)

Layered; do not skip layers (rule in `docs/ARCHITECTURE.md` section 4):

- L1 transport: `protocol.py` (COBS + CRC16-CCITT + pyserial).
- L2 commands: `commands.py`, `_responses.py` (frame builders / response parsing).
- L3 core (the stable public API): `radio.py` (the `Radio` class - one big class exposing everything: `set_phy`, `start_rx`/`read_packets`, `transmit*`, `configure_prop`, TX test modes, and AES/SHA/ECDH/ECDSA crypto), plus `enums.py`, `presets.py`, `exceptions.py`. BLE PHY raw capture stays (`PHY.BLE_1M`/`BLE_2M`/`BLE_CODED_S8`/`BLE_CODED_S2`); the BLE connection/GATT protocol stack was removed 2026-07-20 (Sniffle handles BLE).
- L4 features: `emulation/` (IEEE154, OOK, sub-1GHz device emulation), `_jamming.py`, `_spectrum.py`.
- Integrations: `integrations/killerbee.py` - exposes a CatSniffer as a KillerBee IEEE 802.15.4 device (`killerbee` is an optional, lazily imported dependency). Note `reset_on_init` defaults to False: on the stock RP2040 passthrough firmware the reset cycle can break `init()`, so it stays opt-in.

`feralrf/__init__.py` marks API stability tiers (STABLE / EXPERIMENTAL / PENDING command sets). Public surface is `Radio` + dataclasses; features build on L3 and never bypass to L2/L1.

## Wire Protocol

COBS-framed binary with CRC16-CCITT (poly 0x1021). The framing implemented in `python/feralrf/protocol.py` (`build_frame`, `crc16_ccitt`) is the source of truth; `docs/protocol.md` documents it. The authoritative command/response IDs are the `Command` and `Response` IntEnums in `python/feralrf/enums.py` and the firmware's `command_processor.c` - keep those two in sync when adding a command.

- Command IDs (host -> device): `0x01`-`0x62`, not contiguous. Several IDs in the `0x09`-`0x52` range were the BLE connection/GATT protocol commands (`SET_BLE_ADDR`, `SET_BLE_SCAN_MODE`, `CONNECT`, `GATT_*`, `FOLLOW_*`, `BLE_ADV_LEGACY`, etc.) and were retired when the BLE stack was removed 2026-07-20; those numeric IDs are not reused.
- Response IDs (device -> host): `0x80`-`0xFF`. Key ones: `ACK 0x80`, `ERROR 0x81`, `RX_PACKET 0x90`.

## CC1352 firmware layout (`firmware/cc1352/src/`)

Layered radio engine (full table in `docs/ARCHITECTURE.md` section 2). Notable modules: `radio_if.c`/`phy_manager.c` (PHY abstraction + SmartRF configs), `ll_manager.c` (kept RX PDU classifier - identifies ADV/SCAN/CONNECT/DATA kinds from raw BLE captures; not part of the deleted protocol stack), `host_if.c`/`protocol.c`/`command_processor.c` (UART + COBS + dispatch), `control_task.c`/`data_task.c` (TI-RTOS tasks). The BLE connection/GATT stack (`ble_conn.c`, `ble_conn_mgr.c`, `ble_conn_pdu.c`, `ll_follower.c`, `att_client.c`, `csa2.c`) was deleted 2026-07-20; BLE PHY (`smartrf_ble5_0.c`) and `crypto_engine.c` are unaffected and stay.

### Critical RF invariants (load-bearing - violating these causes silent hangs)

From `docs/ARCHITECTURE.md` section 5 (and the `ti-rtos-rf-cc1352` skill). Any change to `radio_if.c`/`phy_manager.c` must respect:

- Exactly ONE `RF_Object` for the whole firmware. The RF driver has `N_MAX_CLIENTS=2`; a second object silently hangs `RF_open`. Share the existing handle.
- `RF_open` runs ONCE at boot; NEVER call `RF_close` (it can deadlock on `SemaphoreP_pend`). A PHY switch is `RF_flush + RF_yield` then reconfigure, not close/reopen.
- Use `RF_postCmd` for `CMD_FS`, never `RF_runCmd(FS)` (hangs under TI-RTOS). BLE sets frequency via the command's `.channel` field and must NOT issue `CMD_FS`; IEEE/Prop require `CMD_FS` but via `RF_postCmd`.
- ADV TX must terminate: use `endTrigger=TRIG_REL_START` with a finite `endTime`, never `TRIG_NEVER` (that hangs `RF_runCmd`).

## Hardware Configuration

Full pinout in `hardware/PINOUT.md`. Verified essentials:

- RP2040 <-> CC1352 UART: 921600 baud, hardware flow control OFF (`firmware/cc1352/include/config.h`: `UART_BAUD_RATE 921600`, `UART_HW_FLOW_CONTROL 0`). TX=DIO13, RX=DIO12; RTS/CTS pins (DIO14/15) exist but flow control is disabled in firmware.
- CC1352 status LED: DIO24. RESET_CC driven from the RP2040 (GPIO15). SWD not available (JTAG only on DIO16/17); bootloader via UART ROM BSL.
- RP2040 enumerates as three USB CDC-ACM ports: Cat-Bridge (transparent CC1352 passthrough @ 921600), Cat-LoRa (SX1262 stream/command), Cat-Shell (config shell @ 115200). USB VID:PID `0x1209:0xBABB`.

## Key Constraints

- CC1352 memory: static allocation only, no `malloc`. RX buffer is a 16 KB static circular buffer. FW size budget < 120 KB.
- TX power range: -20 to +20 dBm. Reactive jamming target: < 500 us latency.
- SDK version is fixed at TI SimpleLink CC13xx/CC26xx 8.30.01.01.

## Docs map

- `docs/ARCHITECTURE.md` - authoritative layered design + RF rules.
- `docs/protocol.md` - wire-format contract.
- `docs/PYTHON_API.md` - public API status and usage guidance.
- `docs/VALIDATION_MATRIX.md` - baseline matrix and recommended validation flow.
- `docs/TESTING-ON-LINUX.md` - end-to-end KillerBee integration runbook.

## Conventions

- Output is plain ASCII: no emojis, no em/en dashes, no fancy Unicode (use `->`, `!=`, `>=`, straight quotes). Applies to code, comments, commit messages, and docs.
- Commits are authored as the user's own work: no AI/Claude attribution, co-author trailer, or "generated with" line.
- Branching: one branch per phase (`feature/fN-<slug>`); merge to `main` only after a human checkpoint; annotated tag `v2.0-fN` on close.
