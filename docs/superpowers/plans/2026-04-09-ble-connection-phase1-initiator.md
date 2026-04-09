# BLE Connection Phase 1: CMD_BLE5_INITIATOR Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable FeralRF to initiate BLE connections using CMD_BLE5_INITIATOR, sending CONNECT_IND to a target device and transitioning to connected state.

**Architecture:** Firmware generates CONNECT_IND LLData (access address, CRC init, channel map, hop, interval) internally when host sends `CMD_CONNECT(addr, addr_type)`. The RF core's CMD_BLE5_INITIATOR listens on advertising channels and auto-sends CONNECT_IND when it sees ADV_IND from the target. On success, connection state is stored for Phase 2's data channel mode.

**Tech Stack:** TI-RTOS 7, CC1352P7 RF Driver (SDK 8.30), bt5 CPE patch, COBS/UART protocol

**Reference:** Sniffle `RadioWrapper_initiate()` (RadioWrapper.c:645-737), `RadioTask.c:initiateConn()` (L1426-1435), `sniffle_hw.py:initiate_conn()` (L512-537)

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `include/ble_conn.h` | Create | Connection state struct, public API (init, initiate, getState) |
| `src/ble_conn.c` | Create | CONNECT_IND LLData generation, CMD_BLE5_INITIATOR config, state management |
| `src/smartrf_ble5_0.c` | Modify | Add CMD_BLE5_INITIATOR + CMD_BLE5_MASTER structs and params |
| `include/smartrf_ble5_0.h` | Modify | Export new command externs |
| `src/radio_if.c` | Modify | Add `RadioIF_bleInitiate()` — sets up and runs CMD_BLE5_INITIATOR |
| `include/radio_if.h` | Modify | Export `RadioIF_bleInitiate()` |
| `src/command_processor.c` | Modify | Add CMD_CONNECT (0x40), CMD_DISCONNECT (0x41), CMD_CONN_STATUS (0x42) |
| `CMakeLists.txt` | Modify | Add `src/ble_conn.c` to APP_SOURCES |

---

### Task 1: Add CMD_BLE5_INITIATOR and CMD_BLE5_MASTER SmartRF Structs

**Files:**
- Modify: `firmware/cc1352/src/smartrf_ble5_0.c`
- Modify: `firmware/cc1352/include/smartrf_ble5_0.h`

These structs are needed by the RF core. They follow the exact same pattern as the existing Scanner/ADV commands. Copied from Sniffle's ti_radio_config.c with FeralRF naming conventions.

- [ ] **Step 1: Add initiator and master parameter structs to smartrf_ble5_0.c**

Append before the closing of the file (after line 444, the ADV_AUX block):

```c
/* ── BLE5 Initiator (CMD_BLE5_INITIATOR, 0x1828) ── */
static rfc_ble5InitiatorPar_t s_ble5InitiatorPar = {
    .pRxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x0,
    .rxConfig.bAppendStatus = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .initConfig.bUseWhiteList = 0x0,
    .initConfig.bDynamicWinOffset = 0x0,
    .initConfig.deviceAddrType = 0x0,
    .initConfig.peerAddrType = 0x0,
    .initConfig.bStrictLenFilter = 0x0,
    .initConfig.chSel = 0x0,
    .randomState = 0x0000,
    .backoffCount = 0x0001,
    .backoffPar.logUpperLimit = 0x0,
    .backoffPar.bLastSucceeded = 0x0,
    .backoffPar.bLastFailed = 0x0,
    .connectReqLen = 0x00,
    .pConnectReqData = 0,
    .pDeviceAddress = 0,
    .pWhiteList = 0,
    .connectTime = 0x00000000,
    .maxWaitTimeForAuxCh = 0x0000,
    .timeoutTrigger.triggerType = 0x0,
    .timeoutTrigger.bEnaCmd = 0x0,
    .timeoutTrigger.triggerNo = 0x0,
    .timeoutTrigger.pastTrig = 0x0,
    .endTrigger.triggerType = 0x0,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .timeoutTime = 0x00000000,
    .endTime = 0x00000000,
    .rxStartTime = 0x00000000,
    .rxListenTime = 0x0000,
    .channelNo = 0x00,
    .phyMode = 0x00,
};

static rfc_ble5ScanInitOutput_t s_ble5InitiatorOutput;

rfc_CMD_BLE5_INITIATOR_t Ble5_0_cmdBle5Initiator = {
    .commandNo = 0x1828,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .channel = 0x25,
    .whitening.init = 0x65,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &s_ble5InitiatorPar,
    .pOutput = &s_ble5InitiatorOutput,
    .tx20Power = 0x00000000,
};

/* ── BLE5 Master / Central (CMD_BLE5_MASTER, 0x1822) ── */
static rfc_ble5MasterPar_t s_ble5MasterPar = {
    .pRxQ = 0,
    .pTxQ = 0,
    .rxConfig.bAutoFlushIgnored = 0x0,
    .rxConfig.bAutoFlushCrcErr = 0x0,
    .rxConfig.bAutoFlushEmpty = 0x0,
    .rxConfig.bIncludeLenByte = 0x0,
    .rxConfig.bIncludeCrc = 0x0,
    .rxConfig.bAppendRssi = 0x0,
    .rxConfig.bAppendStatus = 0x0,
    .rxConfig.bAppendTimestamp = 0x0,
    .seqStat.lastRxSn = 0x0,
    .seqStat.lastTxSn = 0x0,
    .seqStat.nextTxSn = 0x0,
    .seqStat.bFirstPkt = 0x0,
    .seqStat.bAutoEmpty = 0x0,
    .seqStat.bLlCtrlTx = 0x0,
    .seqStat.bLlCtrlAckRx = 0x0,
    .seqStat.bLlCtrlAckPending = 0x0,
    .maxNack = 0x00,
    .maxPkt = 0x00,
    .accessAddress = 0x00000000,
    .crcInit0 = 0x00,
    .crcInit1 = 0x00,
    .crcInit2 = 0x00,
    .endTrigger.triggerType = 0x0,
    .endTrigger.bEnaCmd = 0x0,
    .endTrigger.triggerNo = 0x0,
    .endTrigger.pastTrig = 0x0,
    .endTime = 0x00000000,
    .maxRxPktLen = 0xFF,
};

rfc_CMD_BLE5_MASTER_t Ble5_0_cmdBle5Master = {
    .commandNo = 0x1822,
    .status = 0x0000,
    .pNextOp = 0,
    .startTime = 0x00000000,
    .startTrigger.triggerType = 0x0,
    .startTrigger.bEnaCmd = 0x0,
    .startTrigger.triggerNo = 0x0,
    .startTrigger.pastTrig = 0x0,
    .condition.rule = 0x1,
    .condition.nSkip = 0x0,
    .channel = 0x00,
    .whitening.init = 0x40,
    .whitening.bOverride = 0x1,
    .phyMode.mainMode = 0x0,
    .phyMode.coding = 0x0,
    .rangeDelay = 0x00,
    .txPower = 0x0000,
    .pParams = &s_ble5MasterPar,
    .pOutput = 0,
    .tx20Power = 0x00000000,
};
```

- [ ] **Step 2: Add externs to smartrf_ble5_0.h**

Add after the existing ADV_AUX extern (line 21):

```c
extern rfc_CMD_BLE5_INITIATOR_t Ble5_0_cmdBle5Initiator;
extern rfc_CMD_BLE5_MASTER_t Ble5_0_cmdBle5Master;
```

- [ ] **Step 3: Build to verify structs compile**

Run: `cd firmware/cc1352/build && cmake .. && make -j$(nproc) 2>&1 | tail -20`
Expected: Build succeeds (structs are unused but compile fine).

- [ ] **Step 4: Commit**

```bash
git add firmware/cc1352/src/smartrf_ble5_0.c firmware/cc1352/include/smartrf_ble5_0.h
git commit -m "feat: add CMD_BLE5_INITIATOR + CMD_BLE5_MASTER SmartRF structs

Initiator (0x1828) for connection establishment, Master (0x1822) for
data channel communication. Both needed for GATT discovery via raw RF.
Structs follow Sniffle ti_radio_config.c pattern."
```

---

### Task 2: Create ble_conn.h — Connection State and API

**Files:**
- Create: `firmware/cc1352/include/ble_conn.h`

This header defines the connection state struct and the public API. The state is static (single connection, no malloc). This is the contract between ble_conn.c, radio_if.c, and command_processor.c.

- [ ] **Step 1: Create ble_conn.h**

```c
/*
 * FeralRF CC1352 - BLE Connection Manager
 *
 * Manages BLE connection initiation via CMD_BLE5_INITIATOR and
 * connection state for data channel communication (Phase 2).
 * Single connection at a time, static allocation, no encryption.
 */

#ifndef BLE_CONN_H
#define BLE_CONN_H

#include <stdbool.h>
#include <stdint.h>

/* CONNECT_IND LLData layout (BLE Core Spec Vol 6, Part B, 2.3.3.1):
 *   [0..3]   Access Address (4 bytes)
 *   [4..6]   CRC Init (3 bytes, little-endian)
 *   [7]      WinSize
 *   [8..9]   WinOffset (1.25ms units)
 *   [10..11] Interval (1.25ms units)
 *   [12..13] Latency
 *   [14..15] Timeout (10ms units)
 *   [16..20] Channel Map (5 bytes, 37 bits)
 *   [21]     Hop (bits 4:0) | SCA (bits 7:5)
 */
#define BLE_CONN_LLDATA_LEN 22u

/* Connection initiation result codes */
typedef enum {
    BLE_CONN_OK = 0,             /* CONNECT_IND sent, connection established */
    BLE_CONN_ERR_BUSY = 1,       /* Already connected or initiating */
    BLE_CONN_ERR_TIMEOUT = 2,    /* No ADV_IND from target within timeout */
    BLE_CONN_ERR_RF = 3,         /* RF command error */
    BLE_CONN_ERR_NO_SYNC = 4,    /* No sync found */
} BleConn_Result;

/* Connection state — exposed for status queries */
typedef struct {
    uint32_t accessAddr;
    uint32_t crcInit;           /* 24-bit, stored in lower 3 bytes */
    uint8_t  channelMap[5];     /* 37 data channels */
    uint8_t  hopIncrement;      /* 5..16 */
    uint16_t connInterval;      /* 1.25ms units */
    uint16_t supervTimeout;     /* 10ms units */
    uint16_t peripheralLatency;
    uint16_t eventCounter;
    uint8_t  peerAddr[6];
    uint8_t  peerAddrType;      /* 0=public, 1=random */
    uint8_t  ownAddr[6];
    bool     connected;
    bool     initiating;
    bool     useCsa2;           /* true if CSA#2 negotiated */
    uint32_t connTime;          /* RAT timestamp of connection event */
} BleConn_State;

void        BleConn_init(void);
BleConn_Result BleConn_initiate(const uint8_t *peerAddr, uint8_t peerAddrType,
                                 uint16_t connIntervalUnits, uint16_t supervTimeoutUnits);
void        BleConn_disconnect(void);
bool        BleConn_isConnected(void);
bool        BleConn_isInitiating(void);
const BleConn_State *BleConn_getState(void);

#endif /* BLE_CONN_H */
```

- [ ] **Step 2: Build to verify header compiles**

Run: `cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -10`
Expected: Build succeeds (header not yet included anywhere).

- [ ] **Step 3: Commit**

```bash
git add firmware/cc1352/include/ble_conn.h
git commit -m "feat: add ble_conn.h — connection state struct and API

Defines BleConn_State for single BLE connection, result codes,
and public functions for initiate/disconnect/status. Static
allocation, no encryption (cleartext GATT MVP)."
```

---

### Task 3: Create ble_conn.c — LLData Generation and Initiation

**Files:**
- Create: `firmware/cc1352/src/ble_conn.c`
- Modify: `firmware/cc1352/CMakeLists.txt`

This is the core of Phase 1. It generates random connection parameters (access address, CRC init, hop increment), builds the 22-byte CONNECT_IND LLData, configures CMD_BLE5_INITIATOR params, and calls RadioIF to execute the command. The random number generation uses TI's TRNG.

- [ ] **Step 1: Create ble_conn.c**

```c
/*
 * FeralRF CC1352 - BLE Connection Manager
 *
 * Generates CONNECT_IND parameters and configures CMD_BLE5_INITIATOR.
 * Reference: Sniffle RadioWrapper_initiate() (RadioWrapper.c:645-737)
 *            Sniffle sniffle_hw.py:initiate_conn() (L512-537)
 */

#include "ble_conn.h"
#include "radio_if.h"
#include "smartrf_ble5_0.h"

#include <string.h>

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_ble_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_ble_mailbox.h)
#include DeviceFamily_constructPath(driverlib/trng.h)
/* clang-format on */
#include <ti/drivers/rf/RF.h>

/* ── Static state (single connection) ── */
static BleConn_State s_state;
static uint8_t s_ll_data[BLE_CONN_LLDATA_LEN];
static uint16_t s_own_addr_u16[3]; /* 16-bit aligned for RF core */
static uint16_t s_peer_addr_u16[3]; /* 16-bit aligned for RF core */

/* ── Random number helpers ── */
static uint32_t ble_conn_rand32(void)
{
    /* Use TRNG for cryptographic-quality randomness.
     * TRNGNumberGet returns a 64-bit value; we take the lower 32 bits. */
    TRNGEnable();
    while (!(TRNGStatusGet() & TRNG_NUMBER_READY)) {
        /* spin — TRNG produces a number in ~1us */
    }
    uint32_t val = (uint32_t)TRNGNumberGet(TRNG_LOW_WORD);
    TRNGDisable();
    return val;
}

static uint8_t ble_conn_rand_hop(void)
{
    /* BLE spec: hop increment must be 5..16 */
    return (uint8_t)(5u + (ble_conn_rand32() % 12u));
}

/* ── LLData builder ── */
static void ble_conn_build_ll_data(uint16_t interval, uint16_t timeout)
{
    uint32_t aa = ble_conn_rand32();
    uint32_t crc = ble_conn_rand32() & 0x00FFFFFFu;
    uint8_t hop = ble_conn_rand_hop();
    uint16_t win_offset = (uint16_t)(5u + (ble_conn_rand32() % 11u)); /* 5..15 */

    /* Access Address (4 bytes LE) */
    s_ll_data[0] = (uint8_t)(aa & 0xFFu);
    s_ll_data[1] = (uint8_t)((aa >> 8) & 0xFFu);
    s_ll_data[2] = (uint8_t)((aa >> 16) & 0xFFu);
    s_ll_data[3] = (uint8_t)((aa >> 24) & 0xFFu);

    /* CRC Init (3 bytes LE) */
    s_ll_data[4] = (uint8_t)(crc & 0xFFu);
    s_ll_data[5] = (uint8_t)((crc >> 8) & 0xFFu);
    s_ll_data[6] = (uint8_t)((crc >> 16) & 0xFFu);

    /* WinSize: 3 (3 * 1.25ms = 3.75ms) */
    s_ll_data[7] = 3u;

    /* WinOffset (1.25ms units) */
    s_ll_data[8] = (uint8_t)(win_offset & 0xFFu);
    s_ll_data[9] = (uint8_t)((win_offset >> 8) & 0xFFu);

    /* Interval (1.25ms units) */
    s_ll_data[10] = (uint8_t)(interval & 0xFFu);
    s_ll_data[11] = (uint8_t)((interval >> 8) & 0xFFu);

    /* Latency: 0 (no peripheral latency for discovery) */
    s_ll_data[12] = 0u;
    s_ll_data[13] = 0u;

    /* Timeout (10ms units) */
    s_ll_data[14] = (uint8_t)(timeout & 0xFFu);
    s_ll_data[15] = (uint8_t)((timeout >> 8) & 0xFFu);

    /* Channel Map: all 37 data channels enabled */
    s_ll_data[16] = 0xFFu;
    s_ll_data[17] = 0xFFu;
    s_ll_data[18] = 0xFFu;
    s_ll_data[19] = 0xFFu;
    s_ll_data[20] = 0x1Fu;

    /* Hop (bits 4:0) | SCA=0 (bits 7:5) */
    s_ll_data[21] = hop & 0x1Fu;

    /* Store in connection state */
    s_state.accessAddr = aa;
    s_state.crcInit = crc;
    memcpy(s_state.channelMap, &s_ll_data[16], 5);
    s_state.hopIncrement = hop;
    s_state.connInterval = interval;
    s_state.supervTimeout = timeout;
    s_state.peripheralLatency = 0;
    s_state.eventCounter = 0;
}

/* ── Public API ── */

void BleConn_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    memset(s_ll_data, 0, sizeof(s_ll_data));

    /* Default own address: use the BLE adv address from radio_if
     * (random static, set by CMD_SET_BLE_ADDR or default 0xAA:BB:CC:DD:EE:01) */
    s_state.ownAddr[0] = 0x01u;
    s_state.ownAddr[1] = 0xEEu;
    s_state.ownAddr[2] = 0xDDu;
    s_state.ownAddr[3] = 0xCCu;
    s_state.ownAddr[4] = 0xBBu;
    s_state.ownAddr[5] = 0xAAu;
}

BleConn_Result BleConn_initiate(const uint8_t *peerAddr, uint8_t peerAddrType,
                                 uint16_t connIntervalUnits, uint16_t supervTimeoutUnits)
{
    if (s_state.connected || s_state.initiating) {
        return BLE_CONN_ERR_BUSY;
    }

    /* Store peer address */
    memcpy(s_state.peerAddr, peerAddr, 6);
    s_state.peerAddrType = peerAddrType;

    /* Build 22-byte CONNECT_IND LLData */
    ble_conn_build_ll_data(connIntervalUnits, supervTimeoutUnits);

    /* Prepare 16-bit aligned address arrays for RF core */
    memcpy(s_own_addr_u16, s_state.ownAddr, 6);
    memcpy(s_peer_addr_u16, peerAddr, 6);

    s_state.initiating = true;

    /* ── Configure CMD_BLE5_INITIATOR ── */
    /* Channel 37 (first adv channel), 1M PHY */
    Ble5_0_cmdBle5Initiator.channel = 37;
    Ble5_0_cmdBle5Initiator.whitening.init = 0x40 + 37;
    Ble5_0_cmdBle5Initiator.phyMode.mainMode = 0; /* 1M */
    Ble5_0_cmdBle5Initiator.phyMode.coding = 0;

    /* RX config — match Sniffle pattern */
    Ble5_0_cmdBle5Initiator.pParams->pRxQ = &s_state; /* will be set by RadioIF */
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAutoFlushIgnored = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAutoFlushCrcErr = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAutoFlushEmpty = 0;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bIncludeLenByte = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bIncludeCrc = 0;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAppendRssi = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAppendStatus = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAppendTimestamp = 1;

    /* Initiator config */
    Ble5_0_cmdBle5Initiator.pParams->initConfig.bUseWhiteList = 0;
    Ble5_0_cmdBle5Initiator.pParams->initConfig.bDynamicWinOffset = 1;
    Ble5_0_cmdBle5Initiator.pParams->initConfig.deviceAddrType = 1; /* random */
    Ble5_0_cmdBle5Initiator.pParams->initConfig.peerAddrType = peerAddrType;
    Ble5_0_cmdBle5Initiator.pParams->initConfig.bStrictLenFilter = 1;
    Ble5_0_cmdBle5Initiator.pParams->initConfig.chSel = 1; /* CSA#2 */

    Ble5_0_cmdBle5Initiator.pParams->randomState = 0;
    Ble5_0_cmdBle5Initiator.pParams->connectReqLen = BLE_CONN_LLDATA_LEN;
    Ble5_0_cmdBle5Initiator.pParams->pConnectReqData = s_ll_data;
    Ble5_0_cmdBle5Initiator.pParams->pDeviceAddress = s_own_addr_u16;
    Ble5_0_cmdBle5Initiator.pParams->pWhiteList =
        (rfc_bleWhiteListEntry_t *)s_peer_addr_u16;

    Ble5_0_cmdBle5Initiator.pParams->connectTime =
        RF_getCurrentTime() + 4000u; /* ~1ms from now */
    Ble5_0_cmdBle5Initiator.pParams->maxWaitTimeForAuxCh = 0xFFFFu;

    /* Run forever until we connect or host cancels */
    Ble5_0_cmdBle5Initiator.pParams->endTrigger.triggerType = TRIG_NEVER;
    Ble5_0_cmdBle5Initiator.pParams->endTime = 0;
    Ble5_0_cmdBle5Initiator.pParams->timeoutTrigger.triggerType = TRIG_NEVER;
    Ble5_0_cmdBle5Initiator.pParams->timeoutTime = 0;

    /* Run the initiator command via RadioIF */
    int result = RadioIF_bleInitiate();

    s_state.initiating = false;

    if (result >= 0) {
        s_state.connected = true;
        s_state.useCsa2 = (result >= 1);
        s_state.connTime = Ble5_0_cmdBle5Initiator.pParams->connectTime;
        return BLE_CONN_OK;
    }

    /* Map Sniffle-style return codes */
    if (result == -1) {
        return BLE_CONN_ERR_TIMEOUT;
    }
    if (result == -2) {
        return BLE_CONN_ERR_NO_SYNC;
    }
    return BLE_CONN_ERR_RF;
}

void BleConn_disconnect(void)
{
    if (s_state.initiating) {
        /* Cancel initiator command */
        RadioIF_stopRx(); /* reuses the cancel mechanism */
    }

    s_state.connected = false;
    s_state.initiating = false;
    s_state.eventCounter = 0;
}

bool BleConn_isConnected(void)
{
    return s_state.connected;
}

bool BleConn_isInitiating(void)
{
    return s_state.initiating;
}

const BleConn_State *BleConn_getState(void)
{
    return &s_state;
}
```

- [ ] **Step 2: Add ble_conn.c to CMakeLists.txt**

In `firmware/cc1352/CMakeLists.txt`, add `src/ble_conn.c` to APP_SOURCES (after `src/command_processor.c`, line 126):

```cmake
set(APP_SOURCES
    ccfg.c
    src/ble_conn.c
    src/command_processor.c
    ...
```

- [ ] **Step 3: Build to verify — expect linker error for RadioIF_bleInitiate**

Run: `cd firmware/cc1352/build && cmake .. && make -j$(nproc) 2>&1 | tail -20`
Expected: Linker error `undefined reference to 'RadioIF_bleInitiate'` — this confirms ble_conn.c compiles and links to the right target. The missing function is added in Task 4.

- [ ] **Step 4: Commit (with build note)**

```bash
git add firmware/cc1352/src/ble_conn.c firmware/cc1352/include/ble_conn.h firmware/cc1352/CMakeLists.txt
git commit -m "feat: add ble_conn.c — CONNECT_IND generation and initiation logic

Generates random access address, CRC init, hop increment via TRNG.
Builds 22-byte LLData per BLE Core Spec Vol 6 Part B 2.3.3.1.
Configures CMD_BLE5_INITIATOR params following Sniffle pattern.
Note: RadioIF_bleInitiate() not yet implemented (next task)."
```

---

### Task 4: Add RadioIF_bleInitiate() to radio_if

**Files:**
- Modify: `firmware/cc1352/src/radio_if.c`
- Modify: `firmware/cc1352/include/radio_if.h`

This function ensures the RF is in BLE mode, sets the RX queue on the initiator command, and runs CMD_BLE5_INITIATOR. It follows the Sniffle pattern: `RF_runCmd(bleRfHandle, &RF_cmdBle5Initiator, ...)` — blocking until connect or cancel.

- [ ] **Step 1: Add RadioIF_bleInitiate() declaration to radio_if.h**

Add after the `RadioIF_setBleAdvAddress` declaration (around line 73):

```c
/* BLE connection initiation — runs CMD_BLE5_INITIATOR (blocking) */
int RadioIF_bleInitiate(void);
```

- [ ] **Step 2: Add RadioIF_bleInitiate() implementation to radio_if.c**

Add near the end of radio_if.c, before the jam session functions (around line 2200, after the existing BLE TX functions). The exact location should be after the last BLE-related function and before `RadioIF_startJamSession()`.

```c
/* ── BLE Connection Initiation ── */

int RadioIF_bleInitiate(void)
{
    RF_EventMask events;

    /* Ensure RF is in BLE mode */
    if (s_rf_mode != RADIO_IF_RF_MODE_BLE) {
        if (!RadioIF_switchRfMode(&Ble5_0_mode,
                                  (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup)) {
            return -3;
        }
        s_rf_mode = RADIO_IF_RF_MODE_BLE;
    }

    /* Stop any active RX */
    if (s_rx_running) {
        RadioIF_stopRx();
    }

    /* Point initiator RX queue to our data queue */
    Ble5_0_cmdBle5Initiator.pParams->pRxQ = &s_rf_data_queue;

    /* Reset command status */
    Ble5_0_cmdBle5Initiator.status = 0;

    /* Run CMD_BLE5_INITIATOR — blocks until CONNECT_IND sent or cancelled.
     * NO CMD_FS needed for BLE (channel field handles frequency).
     * Uses RF_runCmd per Sniffle pattern (RadioWrapper.c:707). */
    events = RF_runCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdBle5Initiator,
                       RF_PriorityNormal, &RadioIF_rfCallback,
                       RF_EventRxEntryDone);

    (void)events;

    /* Map status to result code (Sniffle RadioWrapper.c:719-736) */
    switch (Ble5_0_cmdBle5Initiator.status) {
    case BLE_DONE_CONNECT:
        if (Ble5_0_cmdBle5Initiator.pParams->rxListenTime != 0) {
            return 2; /* AUX connect */
        }
        return 1; /* CSA#2 legacy connect */

    case BLE_DONE_CONNECT_CHSEL0:
        return 0; /* CSA#1 connect */

    case BLE_DONE_RXTIMEOUT:
    case BLE_DONE_ENDED:
    case BLE_DONE_STOPPED:
        return -1; /* timeout / cancelled */

    case BLE_DONE_NOSYNC:
        return -2; /* no sync */

    default:
        return -3; /* RF error */
    }
}
```

- [ ] **Step 3: Build to verify full compilation**

Run: `cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -20`
Expected: Build succeeds with no errors.

- [ ] **Step 4: Commit**

```bash
git add firmware/cc1352/src/radio_if.c firmware/cc1352/include/radio_if.h
git commit -m "feat: add RadioIF_bleInitiate() — runs CMD_BLE5_INITIATOR

Ensures BLE mode, stops active RX, runs blocking initiator command.
Returns Sniffle-compatible status codes: >=0 = connected, <0 = error.
No CMD_FS needed — BLE commands handle frequency via channel field."
```

---

### Task 5: Add CMD_CONNECT / CMD_DISCONNECT / CMD_CONN_STATUS to Command Processor

**Files:**
- Modify: `firmware/cc1352/src/command_processor.c`

Add the three new UART commands that let the Python host control BLE connections. The CONNECT command is async-blocking from the firmware's perspective (the RF_runCmd blocks until connect or cancel), but the host gets an immediate ACK followed by a connection result response.

**Important design note:** Since `BleConn_initiate()` calls `RF_runCmd()` which blocks the calling task, and commands are processed in the UART task context, the initiation will block command processing until connection completes or is cancelled. This is acceptable for Phase 1 MVP — Phase 2 will move this to a separate task if needed.

- [ ] **Step 1: Add new command and response IDs**

After the existing defines in command_processor.c (after line 37, `CMD_JAM_STOP`):

```c
/* BLE Connection commands */
#define CMD_CONNECT        0x40u
#define CMD_DISCONNECT     0x41u
#define CMD_CONN_STATUS    0x42u

/* BLE Connection responses */
#define RSP_CONN_ESTABLISHED 0x94u
#define RSP_DISCONNECTED     0x95u
#define RSP_CONN_STATUS      0x96u
```

**Wait** — RSP_INFO is already 0x94. Fix: use the IDs from the spec:

```c
/* BLE Connection responses (0x94 reserved for RSP_INFO, start at 0xA0) */
#define RSP_CONN_RESULT    0xA0u
#define RSP_CONN_STATUS    0xA1u
```

Actually, looking at the spec more carefully, the spec says RSP_CONN_ESTABLISHED = 0x94 but RSP_INFO already uses 0x94. We need to reassign. Use 0xA0+ range to avoid collisions:

```c
/* BLE Connection commands */
#define CMD_CONNECT        0x40u
#define CMD_DISCONNECT     0x41u
#define CMD_CONN_STATUS    0x42u

/* BLE Connection responses */
#define RSP_CONN_RESULT    0xA0u
#define RSP_CONN_STATUS_R  0xA1u
```

- [ ] **Step 2: Add includes and command handlers**

Add `#include "ble_conn.h"` after the existing includes (after line 16).

Add new cases in `handle_command()` switch, before the `default:` case (before line 356):

```c
    case CMD_CONNECT: {
        /* Payload: addr[6] + addr_type(1) = 7 bytes */
        if (payload_len != 7u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        if (BleConn_isConnected() || BleConn_isInitiating()) {
            send_error(seq, ERR_INVALID_STATE);
            return;
        }
        /* Default: interval=24 (30ms), timeout=100 (1000ms) */
        BleConn_Result res = BleConn_initiate(payload, payload[6], 24u, 100u);
        uint8_t rsp[1] = {(uint8_t)res};
        send_response(RSP_CONN_RESULT, seq, rsp, sizeof(rsp));
        return;
    }

    case CMD_DISCONNECT:
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        BleConn_disconnect();
        send_ack(seq);
        return;

    case CMD_CONN_STATUS: {
        if (payload_len != 0u) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        const BleConn_State *st = BleConn_getState();
        uint8_t rsp[5];
        rsp[0] = st->connected ? 1u : 0u;
        rsp[1] = (uint8_t)(st->connInterval & 0xFFu);
        rsp[2] = (uint8_t)((st->connInterval >> 8) & 0xFFu);
        rsp[3] = (uint8_t)(st->supervTimeout & 0xFFu);
        rsp[4] = (uint8_t)((st->supervTimeout >> 8) & 0xFFu);
        send_response(RSP_CONN_STATUS_R, seq, rsp, sizeof(rsp));
        return;
    }
```

- [ ] **Step 3: Add BleConn_init() call**

In `firmware/cc1352/src/control_task.c`, add `#include "ble_conn.h"` and call `BleConn_init()` inside `ControlTask_init()` or `ControlTask_onRadioInit()`. Find where `RadioIF_init()` is called and add `BleConn_init()` after it.

- [ ] **Step 4: Build**

Run: `cd firmware/cc1352/build && make -j$(nproc) 2>&1 | tail -20`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```bash
git add firmware/cc1352/src/command_processor.c firmware/cc1352/src/control_task.c
git commit -m "feat: add CMD_CONNECT/DISCONNECT/CONN_STATUS commands

CMD_CONNECT (0x40): 7-byte payload (addr[6] + type), runs initiator.
CMD_DISCONNECT (0x41): cancels connection/initiation.
CMD_CONN_STATUS (0x42): returns connected flag + interval + timeout.
RSP_CONN_RESULT (0xA0) reports success/failure with BleConn_Result."
```

---

### Task 6: Flash and Hardware Test

**Files:** None (testing only)

- [ ] **Step 1: Build final firmware**

Run: `cd firmware/cc1352/build && cmake .. && make -j$(nproc) 2>&1 | tail -20`
Expected: Build succeeds, produces `feralrf_cc1352.hex`

- [ ] **Step 2: Flash to CatSniffer**

Run: `catnip -p /dev/ttyACM0 flash firmware/cc1352/build/feralrf_cc1352.hex`
If flash fails, retry once more before asking user to reset.
Expected: Flash succeeds.

- [ ] **Step 3: Verify basic functionality still works**

Open Python shell and test existing commands haven't regressed:

```python
cd python && source .venv/bin/activate
python -c "
from feralrf import FeralRF
import time
rf = FeralRF('/dev/ttyACM1')
rf.open()
info = rf.get_info()
print('INFO:', info)
rf.set_phy(0)  # BLE 1M
rf.start_rx()
time.sleep(2)
rf.stop_rx()
rf.close()
print('Basic BLE RX: OK')
"
```

Expected: Info response received, BLE RX starts/stops without error.

- [ ] **Step 4: Test CMD_CONNECT with Soundcore speaker**

Turn on the Soundcore speaker (Fast Pair 0x8F95F8). Use raw command sending:

```python
python -c "
from feralrf import FeralRF
import time
rf = FeralRF('/dev/ttyACM1')
rf.open()
rf.set_phy(0)  # BLE 1M

# First scan to find the Soundcore's address
rf.start_rx()
time.sleep(3)
# Look for packets from Soundcore in the output
rf.stop_rx()

# Then try connect (will need actual address from scan)
# addr = bytes.fromhex('XXXXXXXXXXXX')  # Soundcore MAC
# rf.send_raw_command(0x40, addr + b'\x01')  # random addr type
rf.close()
"
```

Expected: For Phase 1, the key test is that CMD_CONNECT doesn't crash the firmware. If the Soundcore is advertising, we should see either RSP_CONN_RESULT with BLE_CONN_OK (0) or BLE_CONN_ERR_TIMEOUT (2).

- [ ] **Step 5: Commit any fixes from testing**

If any issues found during testing, fix and commit separately.

---

## Post-Phase 1 Notes

After Phase 1 is validated:
- **Phase 2** (next plan): Add `ble_conn_manager.c` with connection event loop using CMD_BLE5_MASTER, CSA#2 channel selection (`csa2.c` from Sniffle GPLv3), and TX queue for data PDUs.
- **Phase 2 depends on Phase 1:** It uses `BleConn_State.accessAddr`, `.crcInit`, `.connTime`, `.useCsa2` to configure CMD_BLE5_MASTER.
- The `Ble5_0_cmdBle5Master` struct added in Task 1 is not used until Phase 2 but is included now to avoid touching SmartRF config again.
