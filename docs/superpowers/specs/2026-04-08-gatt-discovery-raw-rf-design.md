# GATT Discovery via Raw RF Commands — Design Spec

**Date:** 2026-04-08
**Branch:** feature/ti-rtos-migration
**Base commit:** 9b3b714

## Problem

FeralRF needs GATT service discovery (scan → connect → discover services/characteristics) on CC1352P7. The TI BLE5-Stack (ICall/OneLib.a) approach failed due to ABI incompatibility between GCC ARM and ticlang-compiled libraries. Sniffle proves that full BLE connection management works with raw RF commands, no BLE5-Stack needed.

## Architecture

```
Host (Python) ←COBS/UART→ CC1352 Firmware
                              │
                    ┌─────────┴──────────┐
                    │                    │
              UART Task (P3)       RF Task (P3)
              ├─ CommandProcessor   ├─ RadioIF (RF driver)
              ├─ HostIF (COBS)      ├─ DataTask (RX processing)
              └─ ControlTask        ├─ BleConnManager (NEW)
                                    ├─ AttClient (NEW)
                                    └─ TxQueue (NEW)
```

No ICall. No OneLib.a. No StackWrapper.a. Pure RF Driver + manual BLE protocol.

## Existing Capabilities (commit 9b3b714)

| Feature | Status | RF Command |
|---------|--------|------------|
| Passive BLE RX | Working | CMD_BLE5_GENERIC_RX |
| Active scanning | Working | CMD_BLE5_SCANNER |
| BLE ADV TX (1M) | Working | CMD_BLE5_ADV_NC |
| Extended ADV TX (2M) | Working | CMD_BLE5_ADV_EXT + ADV_AUX |
| PDU classification | Working | LLManager (ADV/SCAN/CONNECT/DATA) |
| PHY selection | Working | 1M/2M/Coded S2/S8 |
| 8 PHYs (non-BLE) | Working | Prop, IEEE, 433, OOK, etc. |

## New Components

### Phase 1: Connection Initiation

**New file:** `src/ble_conn.c` / `include/ble_conn.h`

**RF command:** `CMD_BLE5_INITIATOR` (0x1828) — already in SmartRF config but unused.

**Connection parameters generated at initiation:**
- Access Address: 32-bit random (not 0x8E89BED6, not all-zeros/ones)
- CRC Init: 24-bit random
- Channel Map: 0x1FFFFFFFFF (all 37 data channels)
- Hop Increment: random 5..16
- Connection Interval: 30ms (24 × 1.25ms) — fast for discovery
- Supervision Timeout: 1000ms (100 × 10ms)
- Peripheral Latency: 0

**State struct:**
```c
typedef struct {
    uint32_t accessAddr;
    uint32_t crcInit;       // 24-bit
    uint8_t  channelMap[5]; // 37 channels
    uint8_t  hopIncrement;
    uint16_t connInterval;  // 1.25ms units
    uint16_t supervTimeout; // 10ms units
    uint16_t eventCounter;
    uint8_t  currentChannel;
    uint8_t  peerAddr[6];
    uint8_t  peerAddrType;
    bool     connected;
} BleConn_State;
```

**Flow:**
1. Host sends `CMD_CONNECT(addr[6], addr_type)`
2. Firmware configures `CMD_BLE5_INITIATOR` with peer address + CONNECT_IND params
3. RF core listens on adv channels, auto-sends CONNECT_IND on ADV_IND match
4. On success: store connection state, transition to data channel mode
5. Notify host: `RSP_CONN_ESTABLISHED(handle, interval, timeout)`

**Reference:** Sniffle `RadioWrapper_initiate()` (RadioWrapper.c:645-737)

### Phase 2: Connection State Machine

**New file:** `src/ble_conn_manager.c` — connection event loop

**RF command:** `CMD_BLE5_MASTER` (0x1822)

**Per connection event:**
1. Calculate next data channel via CSA#2 (or CSA#1)
2. Prepare TX queue (ATT request or empty PDU for keepalive)
3. Run `RF_cmdBle5Master` with absolute end time
4. Process RX data (ATT response or LL control)
5. Update event counter, hop to next channel
6. Schedule next event at `currentTime + connInterval`

**Channel Selection Algorithm #2 (CSA#2):**
- Copy from Sniffle `csa2.c` (MIT license, ~100 lines)
- Input: event counter, channel identifier, channel map
- Output: data channel 0-36

**TX Queue:**
- 8-entry circular buffer (like Sniffle TXQueue.c)
- Each entry: LLID (2 bits) + length + payload (max 251 bytes)
- Radio core manages SN/NESN automatically via seqStat

**LL Control PDU handling (minimum):**
- `LL_TERMINATE_IND` (0x02): disconnect, notify host
- `LL_CHANNEL_MAP_IND` (0x01): update channel map
- `LL_CONNECTION_UPDATE_IND` (0x00): update timing (optional for MVP)
- `LL_FEATURE_REQ/RSP` (0x08/0x09): respond with empty feature set
- `LL_VERSION_IND` (0x0C): respond with BLE 5.0 version
- All others: ignore or respond with LL_UNKNOWN_RSP (0x07)

**Connection supervision:**
- Timer tracks last valid RX
- If no valid RX for supervTimeout → disconnect, notify host

**Reference:** Sniffle `RadioWrapper_central()` (RadioWrapper.c:444-510), `RadioTask.c` state machine

### Phase 3: ATT/GATT Protocol

**New file:** `src/att_client.c` / `include/att_client.h`

**L2CAP framing:**
- All ATT PDUs on CID 0x0004
- Header: [Length(2)] [CID(2)] [ATT opcode] [params...]
- LLID=2 for start of L2CAP, LLID=1 for continuation

**ATT operations needed for GATT discovery:**

| ATT Opcode | Name | Purpose |
|------------|------|---------|
| 0x02 | Exchange MTU Request | Negotiate MTU (default 23) |
| 0x03 | Exchange MTU Response | — |
| 0x10 | Read By Group Type Request | Discover primary services (UUID 0x2800) |
| 0x11 | Read By Group Type Response | — |
| 0x08 | Read By Type Request | Discover characteristics (UUID 0x2803) |
| 0x09 | Read By Type Response | — |
| 0x04 | Find Information Request | Discover descriptors |
| 0x05 | Find Information Response | — |
| 0x0A | Read Request | Read characteristic value |
| 0x0B | Read Response | — |
| 0x12 | Write Request | Write characteristic value |
| 0x13 | Write Response | — |
| 0x01 | Error Response | Handle errors |

**GATT discovery sequence:**
1. Exchange MTU (optional, default 23 works)
2. Read By Group Type (0x2800) handles 0x0001-0xFFFF → list of services
3. For each service: Read By Type (0x2803) → list of characteristics
4. For each characteristic: Find Information → descriptors (CCCD, etc.)

**Host response format:**
```
RSP_GATT_SERVICE:  [start_handle(2)][end_handle(2)][uuid_len(1)][uuid(2|16)]
RSP_GATT_CHAR:     [handle(2)][properties(1)][value_handle(2)][uuid_len(1)][uuid(2|16)]
RSP_GATT_DESC:     [handle(2)][uuid(2)]
RSP_GATT_READ:     [handle(2)][data...]
RSP_GATT_WRITE_OK: [handle(2)]
```

### Phase 4: Host API Commands

**New UART commands:**

| Command | ID | Payload | Response |
|---------|-----|---------|----------|
| CONNECT | 0x40 | addr[6], addr_type(1) | RSP_CONN_ESTABLISHED or RSP_ERROR |
| DISCONNECT | 0x41 | — | RSP_DISCONNECTED |
| CONN_STATUS | 0x42 | — | RSP_CONN_STATUS(connected, interval, rssi) |
| GATT_DISCOVER | 0x43 | — | Multiple RSP_GATT_SERVICE |
| GATT_CHARS | 0x44 | start(2), end(2) | Multiple RSP_GATT_CHAR |
| GATT_READ | 0x45 | handle(2) | RSP_GATT_READ |
| GATT_WRITE | 0x46 | handle(2), data... | RSP_GATT_WRITE_OK |

**New response IDs:**

| Response | ID | Payload |
|----------|-----|---------|
| RSP_CONN_ESTABLISHED | 0x94 | interval(2), timeout(2) |
| RSP_DISCONNECTED | 0x95 | reason(1) |
| RSP_CONN_STATUS | 0x96 | connected(1), interval(2) |
| RSP_GATT_SERVICE | 0x97 | start(2), end(2), uuid... |
| RSP_GATT_CHAR | 0x98 | handle(2), props(1), val_handle(2), uuid... |
| RSP_GATT_READ | 0x99 | handle(2), data... |
| RSP_GATT_WRITE_OK | 0x9A | handle(2) |

**Python API:**
```python
async def connect(self, addr: str, addr_type: int = 0) -> bool
async def disconnect(self) -> None
async def discover_services(self) -> list[GattService]
async def discover_characteristics(self, service: GattService) -> list[GattChar]
async def read_characteristic(self, handle: int) -> bytes
async def write_characteristic(self, handle: int, data: bytes) -> bool
```

## Implementation Order

| Phase | Files | Lines est. | Depends on |
|-------|-------|-----------|------------|
| 1 | ble_conn.c/h, smartrf_ble5_0.c (initiator config) | ~200 | — |
| 2 | ble_conn_manager.c, csa2.c, tx_queue.c | ~500 | Phase 1 |
| 3 | att_client.c/h | ~400 | Phase 2 |
| 4 | command_processor.c additions, Python API | ~300 | Phase 3 |

**Total estimated:** ~1400 lines new C code + ~200 lines Python

## Constraints

- Static allocation only (no malloc) — fixed-size connection state, TX queue
- Single connection at a time (simplifies state management)
- No encryption/pairing (cleartext GATT only for MVP)
- No L2CAP fragmentation for MVP (MTU ≤ 23, fits in single PDU)
- Connection interval ≥ 15ms (BLE spec minimum for central)
- Max 1 active BLE connection + existing RF operations (time-division)

## Testing

- Phase 1: Connect to known BLE device (e.g., Soundcore speaker), verify CONNECT_IND sent
- Phase 2: Maintain connection for 10+ seconds, verify channel hopping
- Phase 3: Discover services on Soundcore, compare with nRF Connect
- Phase 4: Python API end-to-end test

## Files NOT needed (removed from ICall approach)

- ble_user_config_feralrf.c — ICall user config
- osal_icall_ble.c — OSAL task registration
- icall_startup_feralrf.c — custom startup_entry
- OneLib.a, StackWrapper.a — precompiled BLE5-Stack
- rom_init.c — ROM jump table
- All 50+ link stubs in rtos_stubs.c for BLE5-Stack symbols
