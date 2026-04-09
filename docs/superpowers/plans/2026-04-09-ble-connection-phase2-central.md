# BLE Connection Phase 2: Central Mode (Data Channel) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Maintain a BLE data channel connection after Phase 1's CONNECT_IND, running CMD_BLE5_MASTER per connection event with channel hopping (CSA#2), empty PDU keepalives, and LL control PDU handling — so the peripheral stays connected for GATT discovery (Phase 3).

**Architecture:** A connection manager (`ble_conn_mgr.c`) polled from the RfTask main loop runs one connection event per poll cycle: compute data channel via CSA#2, prepare TX queue (empty PDU or ATT data), execute CMD_BLE5_MASTER (blocking per-event, ~1-2ms), process RX, advance event counter, sleep until next event. The UartTask remains responsive for CMD_DISCONNECT/CMD_CONN_STATUS.

**Tech Stack:** TI-RTOS 7, CC1352P7 RF Driver (SDK 8.30), bt5 CPE patch, CSA#2 (from Sniffle GPLv3)

**Reference:** Sniffle `RadioWrapper_central()` (RadioWrapper.c:444-510), `RadioTask.c` CENTRAL state (L471-530), `csa2.c`, `TXQueue.c`

**Depends on:** Phase 1 complete (branch `feature/ti-rtos-migration`, commit `fc5c2fe`)

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `include/csa2.h` | Create | CSA#2 API: computeMapping, computeChannel |
| `src/csa2.c` | Create | Channel Selection Algorithm #2 (~80 lines, from Sniffle GPLv3) |
| `include/tx_queue.h` | Create | TX queue API: init, insert, take, flush |
| `src/tx_queue.c` | Create | 8-entry circular TX buffer for BLE data PDUs (~100 lines) |
| `include/ble_conn_mgr.h` | Create | Connection manager API: init, start, poll, stop |
| `src/ble_conn_mgr.c` | Create | Connection event loop, LL control PDU handling, supervision timeout |
| `src/radio_if.c` | Modify | Add `RadioIF_bleCentral()` — runs CMD_BLE5_MASTER for one event |
| `include/radio_if.h` | Modify | Export `RadioIF_bleCentral()` |
| `src/ble_conn.c` | Modify | After successful initiate, call `BleConnMgr_start()` to begin central mode |
| `src/main_rtos.c` | Modify | Add `BleConnMgr_poll()` to RfTask main loop |
| `CMakeLists.txt` | Modify | Add csa2.c, tx_queue.c, ble_conn_mgr.c |

---

### Task 1: Add CSA#2 (Channel Selection Algorithm)

**Files:**
- Create: `firmware/cc1352/include/csa2.h`
- Create: `firmware/cc1352/src/csa2.c`
- Modify: `firmware/cc1352/CMakeLists.txt`

Port from Sniffle's `csa2.c` (GPLv3, compatible with FeralRF GPL-3.0). The algorithm maps (eventCounter, accessAddress, channelMap) → data channel 0-36.

- [ ] **Step 1: Create csa2.h**

```c
/*
 * BLE Channel Selection Algorithm #2 (CSA#2)
 * Based on Sniffle csa2.c — Copyright (c) 2018, NCC Group plc (GPLv3)
 */

#ifndef CSA2_H
#define CSA2_H

#include <stdint.h>

/* Call once after connection established with the connection's AA and channel map.
 * channelMap: 37 bits packed in a uint64_t (bit N = data channel N enabled). */
void csa2_computeMapping(uint32_t accessAddress, uint64_t channelMap);

/* Compute the data channel for a given connection event counter.
 * Returns 0-36. Must call csa2_computeMapping first. */
uint8_t csa2_computeChannel(uint32_t connEventCounter);

#endif /* CSA2_H */
```

- [ ] **Step 2: Create csa2.c**

```c
/*
 * BLE Channel Selection Algorithm #2 (CSA#2)
 * Based on Sniffle csa2.c — Copyright (c) 2018, NCC Group plc (GPLv3)
 */

#include "csa2.h"

static uint64_t csa2_chanMap;
static uint8_t csa2_numUsedChannels;
static uint8_t csa2_remapping_table[37];
static uint16_t channelIdentifier;

/* Compile-time bit reversal table */
#define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
static const uint8_t bitReverseTable[256] = {
    R6(0), R6(2), R6(1), R6(3)
};

static inline uint16_t csa2_perm(uint16_t b) {
    uint8_t byte0 = b & 0xFF;
    uint8_t byte1 = b >> 8;
    return bitReverseTable[byte0] | (bitReverseTable[byte1] << 8);
}

static inline uint16_t csa2_mam(uint16_t a, uint16_t b) {
    uint32_t u = a * 17 + b;
    return u & 0xFFFF;
}

static uint16_t csa2_eprn(uint16_t counter) {
    uint16_t u = counter;
    u ^= channelIdentifier;
    u = csa2_perm(u);
    u = csa2_mam(u, channelIdentifier);
    u = csa2_perm(u);
    u = csa2_mam(u, channelIdentifier);
    u = csa2_perm(u);
    u = csa2_mam(u, channelIdentifier);
    u ^= channelIdentifier;
    return u;
}

void csa2_computeMapping(uint32_t accessAddress, uint64_t map) {
    uint16_t lower = accessAddress & 0xFFFF;
    uint16_t upper = accessAddress >> 16;

    csa2_numUsedChannels = 0;
    for (uint8_t i = 0; i < 37; i++) {
        if (map & (1ULL << i)) {
            csa2_remapping_table[csa2_numUsedChannels] = i;
            csa2_numUsedChannels++;
        }
    }

    channelIdentifier = lower ^ upper;
    csa2_chanMap = map;
}

uint8_t csa2_computeChannel(uint32_t connEventCounter) {
    uint16_t e_prn = csa2_eprn(connEventCounter & 0xFFFF);
    uint8_t mod_eprn = e_prn % 37;

    if (csa2_chanMap & (1ULL << mod_eprn)) {
        return mod_eprn;
    }
    return csa2_remapping_table[(csa2_numUsedChannels * e_prn) >> 16];
}
```

- [ ] **Step 3: Add csa2.c to CMakeLists.txt**

Add `src/csa2.c` to APP_SOURCES (after `src/ble_conn.c`).

- [ ] **Step 4: Build**

Run: `cd firmware/cc1352/build && cmake .. && make -j$(nproc)`
Expected: Build succeeds.

- [ ] **Step 5: Commit**

```
feat: add CSA#2 channel selection algorithm

BLE Channel Selection Algorithm #2, ported from Sniffle (GPLv3).
Maps (eventCounter, accessAddress, channelMap) to data channel 0-36.
Used by central mode for channel hopping during connection events.
```

---

### Task 2: Add TX Queue for BLE Data PDUs

**Files:**
- Create: `firmware/cc1352/include/tx_queue.h`
- Create: `firmware/cc1352/src/tx_queue.c`
- Modify: `firmware/cc1352/CMakeLists.txt`

8-entry circular buffer for TX PDUs. The RF core's CMD_BLE5_MASTER takes a `dataQueue_t` for TX, with entries containing LLID + payload. The radio core manages SN/NESN via seqStat. Adapted from Sniffle TXQueue.c (GPLv3).

- [ ] **Step 1: Create tx_queue.h**

```c
/*
 * BLE TX Queue — circular buffer for data channel PDUs
 * Based on Sniffle TXQueue.c — Copyright (c) 2020-2022, NCC Group plc (GPLv3)
 */

#ifndef TX_QUEUE_H
#define TX_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_data_entry.h)
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
/* clang-format on */

/* LLID values for BLE data PDUs */
#define TX_QUEUE_LLID_DATA_CONT   1u  /* L2CAP continuation or empty PDU */
#define TX_QUEUE_LLID_DATA_START  2u  /* L2CAP start */
#define TX_QUEUE_LLID_CTRL        3u  /* LL control PDU */

void     TXQueue_init(void);
bool     TXQueue_insert(uint8_t len, uint8_t llid, const void *data);
uint32_t TXQueue_take(dataQueue_t *pRFQueue);
void     TXQueue_flush(uint32_t numEntries);

#endif /* TX_QUEUE_H */
```

- [ ] **Step 2: Create tx_queue.c**

```c
/*
 * BLE TX Queue — circular buffer for data channel PDUs
 * Based on Sniffle TXQueue.c — Copyright (c) 2020-2022, NCC Group plc (GPLv3)
 */

#include "tx_queue.h"

#include <string.h>

#define TX_QUEUE_SIZE 8u
#define TX_QUEUE_MASK (TX_QUEUE_SIZE - 1u)
#define TX_QUEUE_PACKET_SIZE 258u /* 255 payload + 1 LLID header + 2 spare */

static uint8_t s_packet_buf[TX_QUEUE_PACKET_SIZE * TX_QUEUE_SIZE];
static rfc_dataEntryPointer_t s_queue_entries[TX_QUEUE_SIZE];

static volatile uint32_t s_head; /* insert here */
static volatile uint32_t s_tail; /* take from here */

void TXQueue_init(void) {
    s_head = 0;
    s_tail = 0;

    for (uint32_t i = 0; i < TX_QUEUE_SIZE; i++) {
        uint32_t next_idx = (i + 1u) & TX_QUEUE_MASK;
        s_queue_entries[i].pNextEntry = (uint8_t *)(s_queue_entries + next_idx);
        s_queue_entries[i].status = DATA_ENTRY_PENDING;
        s_queue_entries[i].config.type = DATA_ENTRY_TYPE_PTR;
        s_queue_entries[i].config.lenSz = 0;
        s_queue_entries[i].length = 0;
        s_queue_entries[i].pData = s_packet_buf + (i * TX_QUEUE_PACKET_SIZE);
    }
}

bool TXQueue_insert(uint8_t len, uint8_t llid, const void *data) {
    if (((s_head - s_tail) & TX_QUEUE_MASK) == TX_QUEUE_MASK) {
        return false; /* full */
    }

    uint32_t h = s_head & TX_QUEUE_MASK;

    if (s_queue_entries[h].status == DATA_ENTRY_ACTIVE ||
        s_queue_entries[h].status == DATA_ENTRY_BUSY) {
        return false;
    }

    s_queue_entries[h].status = DATA_ENTRY_PENDING;
    s_queue_entries[h].length = 1u + len; /* LLID byte + payload */
    uint8_t *pData = s_queue_entries[h].pData;
    *pData = llid & 0x3u;
    if (len > 0 && data != NULL) {
        memcpy(pData + 1, data, len);
    }

    s_head++;
    return true;
}

uint32_t TXQueue_take(dataQueue_t *pRFQueue) {
    uint32_t h = s_head;
    uint32_t t = s_tail;
    uint32_t qsize = (h - t) & TX_QUEUE_MASK;

    if (qsize) {
        uint32_t first = t & TX_QUEUE_MASK;
        uint32_t last = (h - 1u) & TX_QUEUE_MASK;
        pRFQueue->pCurrEntry = (uint8_t *)(s_queue_entries + first);
        pRFQueue->pLastEntry = (uint8_t *)(s_queue_entries + last);
    } else {
        pRFQueue->pCurrEntry = NULL;
        pRFQueue->pLastEntry = NULL;
    }

    return qsize;
}

void TXQueue_flush(uint32_t numEntries) {
    uint32_t qsize = (s_head - s_tail) & TX_QUEUE_MASK;
    if (numEntries > qsize) {
        numEntries = qsize;
    }
    s_tail += numEntries;
}
```

- [ ] **Step 3: Add tx_queue.c to CMakeLists.txt**

Add `src/tx_queue.c` to APP_SOURCES.

- [ ] **Step 4: Build**

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```
feat: add BLE TX queue for data channel PDUs

8-entry circular buffer for CMD_BLE5_MASTER TX. Each entry holds
LLID (2 bits) + payload (max 255 bytes). Radio core manages SN/NESN
via seqStat. Adapted from Sniffle TXQueue.c (GPLv3).
```

---

### Task 3: Add RadioIF_bleCentral() — Run CMD_BLE5_MASTER for One Event

**Files:**
- Modify: `firmware/cc1352/src/radio_if.c`
- Modify: `firmware/cc1352/include/radio_if.h`

This function runs CMD_BLE5_MASTER for a single connection event: sets the data channel, access address, CRC init, TX/RX queues, timing, and executes RF_runCmd. It returns success/failure and the number of TX entries consumed.

- [ ] **Step 1: Add declaration to radio_if.h**

After the existing `RadioIF_bleInitiate()` declaration:

```c
/* BLE central mode — run CMD_BLE5_MASTER for one connection event.
 * chan: data channel 0-36
 * accessAddr: connection access address
 * crcInit: 24-bit CRC init value
 * pTxQueue: TX data queue (from TXQueue_take), can be empty
 * startTime: absolute RAT time to start this event
 * endTime: absolute RAT time to end (startTime + connInterval)
 * pNumSent: output — number of TX entries consumed by RF core
 * Returns: 0 = success (got data), -1 = no response, -2 = RF error */
int RadioIF_bleCentral(uint8_t chan, uint32_t accessAddr, uint32_t crcInit,
                       dataQueue_t *pTxQueue, uint32_t startTime,
                       uint32_t endTime, uint32_t *pNumSent);
```

The `#include` for `dataQueue_t` is needed. Add at top of radio_if.h (it's already available via `rf_mailbox.h` from `RF.h`).

- [ ] **Step 2: Add implementation to radio_if.c**

Place right after `RadioIF_bleInitiate()`. The function follows Sniffle's `RadioWrapper_central()` pattern (RadioWrapper.c:444-510):

```c
int RadioIF_bleCentral(uint8_t chan, uint32_t accessAddr, uint32_t crcInit,
                       dataQueue_t *pTxQueue, uint32_t startTime,
                       uint32_t endTime, uint32_t *pNumSent) {
    rfc_bleMasterSlaveOutput_t output = {0};

    if (s_rf_handle == NULL || chan >= 37) {
        return -2;
    }

    Ble5_0_cmdBle5Master.channel = chan;
    Ble5_0_cmdBle5Master.whitening.init = 0x40 + chan;
    Ble5_0_cmdBle5Master.whitening.bOverride = 1;
    Ble5_0_cmdBle5Master.phyMode.mainMode = 0; /* 1M */
    Ble5_0_cmdBle5Master.phyMode.coding = 0;
    Ble5_0_cmdBle5Master.pOutput = &output;

    Ble5_0_cmdBle5Master.pParams->pRxQ = &s_rf_data_queue;
    Ble5_0_cmdBle5Master.pParams->pTxQ = pTxQueue;
    Ble5_0_cmdBle5Master.pParams->accessAddress = accessAddr;
    Ble5_0_cmdBle5Master.pParams->crcInit0 = crcInit & 0xFF;
    Ble5_0_cmdBle5Master.pParams->crcInit1 = (crcInit >> 8) & 0xFF;
    Ble5_0_cmdBle5Master.pParams->crcInit2 = (crcInit >> 16) & 0xFF;
    Ble5_0_cmdBle5Master.pParams->maxRxPktLen = 0xFF;

    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushIgnored = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushCrcErr = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAutoFlushEmpty = 0;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bIncludeLenByte = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bIncludeCrc = 0;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAppendRssi = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAppendStatus = 1;
    Ble5_0_cmdBle5Master.pParams->rxConfig.bAppendTimestamp = 1;

    /* Absolute start time */
    if (startTime == 0) {
        Ble5_0_cmdBle5Master.startTrigger.triggerType = TRIG_NOW;
    } else {
        Ble5_0_cmdBle5Master.startTrigger.triggerType = TRIG_ABSTIME;
        Ble5_0_cmdBle5Master.startTrigger.pastTrig = 1;
        Ble5_0_cmdBle5Master.startTime = startTime;
    }

    /* Absolute end time — one connection interval */
    Ble5_0_cmdBle5Master.pParams->endTrigger.triggerType = TRIG_ABSTIME;
    Ble5_0_cmdBle5Master.pParams->endTime = endTime;

    Ble5_0_cmdBle5Master.status = 0;

    RF_runCmd(s_rf_handle, (RF_Op *)&Ble5_0_cmdBle5Master, RF_PriorityNormal,
              &RadioIF_rfCallback, RF_EventRxEntryDone);

    *pNumSent = output.nTxEntryDone;

    switch (Ble5_0_cmdBle5Master.status) {
    case BLE_DONE_OK:
    case BLE_DONE_ENDED:
    case BLE_DONE_STOPPED:
        return 0;
    default:
        return -1;
    }
}
```

- [ ] **Step 3: Add seqStat reset function to radio_if**

Add to radio_if.h:
```c
void RadioIF_bleResetSeqStat(void);
```

Add to radio_if.c (after RadioIF_bleCentral):
```c
void RadioIF_bleResetSeqStat(void) {
    Ble5_0_cmdBle5Master.pParams->seqStat.lastRxSn = 1;
    Ble5_0_cmdBle5Master.pParams->seqStat.lastTxSn = 1;
    Ble5_0_cmdBle5Master.pParams->seqStat.nextTxSn = 0;
    Ble5_0_cmdBle5Master.pParams->seqStat.bFirstPkt = 1;
    Ble5_0_cmdBle5Master.pParams->seqStat.bAutoEmpty = 0;
    Ble5_0_cmdBle5Master.pParams->seqStat.bLlCtrlTx = 0;
    Ble5_0_cmdBle5Master.pParams->seqStat.bLlCtrlAckRx = 0;
    Ble5_0_cmdBle5Master.pParams->seqStat.bLlCtrlAckPending = 0;
}
```

- [ ] **Step 4: Build**

Expected: Build succeeds.

- [ ] **Step 5: Commit**

```
feat: add RadioIF_bleCentral() + seqStat reset

Runs CMD_BLE5_MASTER for one connection event with specified
channel, access address, CRC, TX/RX queues, and timing.
Follows Sniffle RadioWrapper_central() pattern (L444-510).
Also adds RadioIF_bleResetSeqStat() for initiator→central transition.
```

---

### Task 4: Create ble_conn_mgr — Connection Event Loop

**Files:**
- Create: `firmware/cc1352/include/ble_conn_mgr.h`
- Create: `firmware/cc1352/src/ble_conn_mgr.c`
- Modify: `firmware/cc1352/CMakeLists.txt`

This is the core of Phase 2. The connection manager runs from the RfTask loop, executing one connection event per `poll()` call. It handles:
- Channel hopping via CSA#2
- TX queue management (empty PDU keepalives)
- Connection event timing
- LL control PDU responses (FEATURE_REQ, VERSION_IND, TERMINATE_IND, CHANNEL_MAP_IND)
- Supervision timeout

- [ ] **Step 1: Create ble_conn_mgr.h**

```c
/*
 * FeralRF CC1352 - BLE Connection Manager (Central Mode)
 *
 * Runs the connection event loop after Phase 1 establishes a connection.
 * Polled from RfTask — one connection event per poll() call.
 */

#ifndef BLE_CONN_MGR_H
#define BLE_CONN_MGR_H

#include <stdbool.h>
#include <stdint.h>

/* Initialize the connection manager (call once at boot) */
void BleConnMgr_init(void);

/* Start central mode after successful BLE_CONN_OK from BleConn_initiate().
 * Sets up CSA#2 mapping, TX queue, initial timing from BleConn_State. */
void BleConnMgr_start(void);

/* Stop and clean up (called on disconnect or supervision timeout) */
void BleConnMgr_stop(void);

/* Poll — call from RfTask main loop. Runs one connection event if due.
 * Returns true if a connection event was executed. */
bool BleConnMgr_poll(void);

/* Returns true if central mode is actively running */
bool BleConnMgr_isRunning(void);

/* Queue a TX PDU for the next connection event.
 * llid: 1=continuation/empty, 2=L2CAP start, 3=LL control
 * Returns false if TX queue is full. */
bool BleConnMgr_queueTx(uint8_t llid, const uint8_t *data, uint8_t len);

#endif /* BLE_CONN_MGR_H */
```

- [ ] **Step 2: Create ble_conn_mgr.c**

```c
/*
 * FeralRF CC1352 - BLE Connection Manager (Central Mode)
 *
 * Reference: Sniffle RadioTask.c CENTRAL state (L471-530)
 *            Sniffle afterConnEvent() (L232-380)
 */

#include "ble_conn_mgr.h"
#include "ble_conn.h"
#include "csa2.h"
#include "radio_if.h"
#include "tx_queue.h"

#include <ti/drivers/rf/RF.h>
#include <ti/sysbios/knl/Task.h>

/* LL Control PDU opcodes (BLE Core Spec Vol 6, Part B, 2.4.2) */
#define LL_CONNECTION_UPDATE_IND 0x00u
#define LL_CHANNEL_MAP_IND       0x01u
#define LL_TERMINATE_IND         0x02u
#define LL_FEATURE_REQ           0x08u
#define LL_FEATURE_RSP           0x09u
#define LL_VERSION_IND           0x0Cu
#define LL_UNKNOWN_RSP           0x07u

/* BLE version: 5.0 = 0x09 */
#define BLE_VERSION_5_0          0x09u
/* TI company ID (for VERSION_IND) */
#define COMPANY_ID_TI            0x000Du
#define SUBVERSION_FERALRF       0x0001u

/* RAT clock: 4 MHz = 4 ticks per us */
#define RAT_TICKS_PER_MS         4000u
#define CONN_INTERVAL_TO_TICKS(x) ((uint32_t)(x) * 5000u) /* 1.25ms units → RAT */
#define SUPERV_TO_TICKS(x)        ((uint32_t)(x) * 40000u) /* 10ms units → RAT */

/* transmitWindowDelay for CONNECT_IND = 1.25ms = 5000 RAT ticks */
#define TRANSMIT_WINDOW_DELAY    5000u
/* Anchor offset — start listening slightly before expected anchor */
#define ANCHOR_OFFSET            800u

static bool s_running;
static uint32_t s_hop_interval_ticks;
static uint32_t s_superv_timeout_ticks;
static uint32_t s_next_hop_time;
static uint32_t s_last_rx_time;      /* for supervision timeout */
static uint16_t s_event_counter;
static bool s_got_first_event;

/* ── LL Control PDU handling ── */

static void handle_ll_ctrl(const uint8_t *payload, uint8_t len) {
    if (len < 1) {
        return;
    }
    uint8_t opcode = payload[0];

    switch (opcode) {
    case LL_TERMINATE_IND:
        /* Peer wants to disconnect */
        BleConnMgr_stop();
        BleConn_disconnect();
        break;

    case LL_FEATURE_REQ: {
        /* Respond with empty feature set */
        uint8_t rsp[9];
        rsp[0] = LL_FEATURE_RSP;
        /* 8 bytes of features — all zeros (no features supported) */
        for (uint8_t i = 1; i < 9; i++) {
            rsp[i] = 0;
        }
        TXQueue_insert(9, TX_QUEUE_LLID_CTRL, rsp);
        break;
    }

    case LL_VERSION_IND: {
        /* Respond with our version */
        uint8_t rsp[6];
        rsp[0] = LL_VERSION_IND;
        rsp[1] = BLE_VERSION_5_0;
        rsp[2] = (uint8_t)(COMPANY_ID_TI & 0xFF);
        rsp[3] = (uint8_t)(COMPANY_ID_TI >> 8);
        rsp[4] = (uint8_t)(SUBVERSION_FERALRF & 0xFF);
        rsp[5] = (uint8_t)(SUBVERSION_FERALRF >> 8);
        TXQueue_insert(6, TX_QUEUE_LLID_CTRL, rsp);
        break;
    }

    case LL_CHANNEL_MAP_IND: {
        /* Update channel map: payload = [opcode(1)][map(5)][instant(2)] */
        if (len >= 8) {
            uint64_t new_map = 0;
            for (uint8_t i = 0; i < 5; i++) {
                new_map |= (uint64_t)payload[1 + i] << (8 * i);
            }
            uint16_t instant = (uint16_t)payload[6] | ((uint16_t)payload[7] << 8);
            /* For MVP: apply immediately (should wait for instant, but close enough) */
            (void)instant;
            const BleConn_State *st = BleConn_getState();
            csa2_computeMapping(st->accessAddr, new_map);
        }
        break;
    }

    default: {
        /* Unknown opcode — respond with LL_UNKNOWN_RSP */
        uint8_t rsp[2];
        rsp[0] = LL_UNKNOWN_RSP;
        rsp[1] = opcode;
        TXQueue_insert(2, TX_QUEUE_LLID_CTRL, rsp);
        break;
    }
    }
}

/* ── Process RX packets from connection event ── */

static bool process_rx_packets(void) {
    bool got_data = false;

    /* Read packets from the RF data queue.
     * RadioIF_bleCentral left received entries in s_rf_data_queue.
     * We use RadioIF_popRxPacket which processes the shared queue. */
    RadioIF_RxPacket pkt;
    while (RadioIF_popRxPacket(&pkt)) {
        got_data = true;

        /* Data PDU header: first byte has LLID in bits 1:0 */
        if (pkt.data_len < 2) {
            continue;
        }
        uint8_t llid = pkt.data[0] & 0x03u;
        uint8_t pdu_len = pkt.data[1];

        if (llid == 3 && pdu_len > 0) {
            /* LL Control PDU */
            handle_ll_ctrl(&pkt.data[2], pdu_len);
        }
        /* llid 1 or 2 = L2CAP data — will be used in Phase 3 (ATT/GATT) */
    }

    return got_data;
}

/* ── Public API ── */

void BleConnMgr_init(void) {
    s_running = false;
    s_event_counter = 0;
    s_got_first_event = false;
    TXQueue_init();
}

void BleConnMgr_start(void) {
    const BleConn_State *st = BleConn_getState();

    if (!st->connected) {
        return;
    }

    s_running = true;
    s_event_counter = 0;
    s_got_first_event = false;

    /* Compute timing from connection parameters */
    s_hop_interval_ticks = CONN_INTERVAL_TO_TICKS(st->connInterval);
    s_superv_timeout_ticks = SUPERV_TO_TICKS(st->supervTimeout);

    /* Compute first hop time: connTime + transmitWindowDelay + WinOffset*1.25ms + interval
     * The WinOffset is in s_ll_data[8..9] but we stored it in the connInterval flow.
     * For simplicity, use connTime + one interval as the first event anchor. */
    s_next_hop_time = st->connTime + TRANSMIT_WINDOW_DELAY + s_hop_interval_ticks;
    s_last_rx_time = RF_getCurrentTime();

    /* Set up CSA#2 channel mapping */
    if (st->useCsa2) {
        uint64_t map = 0;
        for (uint8_t i = 0; i < 5; i++) {
            map |= (uint64_t)st->channelMap[i] << (8 * i);
        }
        csa2_computeMapping(st->accessAddr, map);
    }

    /* Reset TX queue and seqStat for fresh connection */
    TXQueue_init();
    RadioIF_bleResetSeqStat();

    /* Insert an empty PDU as first TX (keepalive) */
    TXQueue_insert(0, TX_QUEUE_LLID_DATA_CONT, NULL);
}

void BleConnMgr_stop(void) {
    s_running = false;
    s_event_counter = 0;
    s_got_first_event = false;
}

bool BleConnMgr_poll(void) {
    if (!s_running) {
        return false;
    }

    const BleConn_State *st = BleConn_getState();
    if (!st->connected) {
        BleConnMgr_stop();
        return false;
    }

    /* Check if it's time for the next connection event */
    uint32_t now = RF_getCurrentTime();
    uint32_t remaining = s_next_hop_time - ANCHOR_OFFSET - now;
    if (remaining < 0x80000000u && remaining > 2000u) {
        /* Not yet time — sleep a bit (RAT ticks / 40 ≈ Task_sleep ticks) */
        Task_sleep(remaining / 40u);
        return false;
    }

    /* Check supervision timeout */
    if (now - s_last_rx_time > s_superv_timeout_ticks) {
        BleConnMgr_stop();
        BleConn_disconnect();
        return false;
    }

    /* Compute data channel */
    uint8_t chan;
    if (st->useCsa2) {
        chan = csa2_computeChannel(s_event_counter);
    } else {
        /* CSA#1 fallback: simple hop */
        chan = (st->hopIncrement * s_event_counter) % 37;
    }

    /* Prepare TX queue for RF core */
    dataQueue_t txq;
    TXQueue_take(&txq);

    /* Ensure at least an empty PDU for keepalive */
    if (txq.pCurrEntry == NULL) {
        TXQueue_insert(0, TX_QUEUE_LLID_DATA_CONT, NULL);
        TXQueue_take(&txq);
    }

    /* Run CMD_BLE5_MASTER for this connection event */
    uint32_t startTime = s_next_hop_time - ANCHOR_OFFSET;
    uint32_t endTime = s_next_hop_time + s_hop_interval_ticks - ANCHOR_OFFSET;
    uint32_t numSent = 0;

    int status = RadioIF_bleCentral(chan, st->accessAddr, st->crcInit,
                                    &txq, startTime, endTime, &numSent);

    TXQueue_flush(numSent);

    /* Process received data */
    bool got_data = process_rx_packets();

    /* Update supervision timer */
    if (got_data || status == 0) {
        s_last_rx_time = RF_getCurrentTime();
    }

    /* Advance to next connection event */
    s_event_counter++;
    s_next_hop_time += s_hop_interval_ticks;

    /* Insert empty PDU for next event keepalive */
    TXQueue_insert(0, TX_QUEUE_LLID_DATA_CONT, NULL);

    return true;
}

bool BleConnMgr_isRunning(void) {
    return s_running;
}

bool BleConnMgr_queueTx(uint8_t llid, const uint8_t *data, uint8_t len) {
    if (!s_running) {
        return false;
    }
    return TXQueue_insert(len, llid, data);
}
```

- [ ] **Step 3: Add ble_conn_mgr.c to CMakeLists.txt**

Add `src/ble_conn_mgr.c` to APP_SOURCES.

- [ ] **Step 4: Build**

Expected: Build succeeds. Some warnings about RadioIF_popRxPacket usage may appear.

- [ ] **Step 5: Commit**

```
feat: add ble_conn_mgr — connection event loop for central mode

Polled from RfTask, runs one connection event per poll():
- CSA#2 channel selection per event counter
- CMD_BLE5_MASTER TX/RX with absolute timing
- Empty PDU keepalives between events
- LL control: FEATURE_REQ, VERSION_IND, TERMINATE_IND, CHANNEL_MAP_IND
- Supervision timeout detection
- LL_UNKNOWN_RSP for unhandled opcodes
```

---

### Task 5: Wire Up — Start Central After Connect, Poll from RfTask

**Files:**
- Modify: `firmware/cc1352/src/ble_conn.c`
- Modify: `firmware/cc1352/src/main_rtos.c`
- Modify: `firmware/cc1352/src/control_task.c`

Three integration points:
1. After `BleConn_initiate()` succeeds → call `BleConnMgr_start()`
2. RfTask main loop → call `BleConnMgr_poll()` each iteration
3. `BleConn_disconnect()` → call `BleConnMgr_stop()`
4. `BleConn_init()` → call `BleConnMgr_init()`

- [ ] **Step 1: Modify ble_conn.c — start central after connect**

Add `#include "ble_conn_mgr.h"` to includes.

In `BleConn_initiate()`, after `s_state.connected = true`:
```c
    if (result >= 0) {
        s_state.connected = true;
        s_state.useCsa2 = (result >= 1);
        s_state.connTime = Ble5_0_cmdBle5Initiator.pParams->connectTime;
        BleConnMgr_start(); /* Begin central mode event loop */
        return BLE_CONN_OK;
    }
```

In `BleConn_disconnect()`, add `BleConnMgr_stop()`:
```c
void BleConn_disconnect(void) {
    BleConnMgr_stop();
    if (s_state.initiating) {
        RadioIF_stopRx();
    }
    s_state.connected = false;
    s_state.initiating = false;
    s_state.eventCounter = 0;
}
```

In `BleConn_init()`, add `BleConnMgr_init()`:
```c
void BleConn_init(void) {
    memset(&s_state, 0, sizeof(s_state));
    memset(s_ll_data, 0, sizeof(s_ll_data));
    s_state.ownAddr[0] = 0x01u;
    ...
    BleConnMgr_init();
}
```

- [ ] **Step 2: Modify main_rtos.c — poll from RfTask**

Add `#include "ble_conn_mgr.h"` to includes.

In `RfTask_taskFxn`, modify the main loop:
```c
    while (1) {
        if (BleConnMgr_isRunning()) {
            BleConnMgr_poll();
        } else {
            DataTask_poll();
        }
        Task_yield();
    }
```

When central mode is running, the connection event loop takes priority over normal DataTask processing. DataTask resumes when the connection ends.

- [ ] **Step 3: Build**

Expected: Build succeeds.

- [ ] **Step 4: Commit**

```
feat: wire up central mode — start after connect, poll from RfTask

BleConnMgr_start() called after successful CONNECT_IND.
BleConnMgr_poll() called from RfTask main loop (replaces DataTask
while connection is active). BleConnMgr_stop() on disconnect.
```

---

### Task 6: Build, Flash, and Hardware Test

**Files:** None (testing only)

- [ ] **Step 1: Build final firmware**

Run: `cd firmware/cc1352/build && cmake .. && make -j$(nproc)`
Expected: Build succeeds.

- [ ] **Step 2: Flash to CatSniffer**

Run: `catnip flash firmware/cc1352/build/feralrf_cc1352.hex`
Retry once on failure.

- [ ] **Step 3: Test connection persistence**

Using nRF Connect on phone as BLE peripheral (legacy advertising, "Sabas"):

```python
# 1. Scan ch37 for connectable device
# 2. CMD_CONNECT
# 3. Monitor — connection should stay alive > 10 seconds
# 4. Check CONN_STATUS periodically
# 5. nRF Connect should show "Connected" state maintained
```

The key test: connection survives past the 1-second supervision timeout (which it didn't before Phase 2). The central mode sends empty PDU keepalives every connection interval (30ms), and the peripheral responds, keeping the link alive.

- [ ] **Step 4: Test disconnect**

Send CMD_DISCONNECT and verify nRF Connect shows "Disconnected".

- [ ] **Step 5: Test supervision timeout**

Connect, then turn off Bluetooth on phone. The firmware should detect supervision timeout (~1s) and auto-disconnect.

- [ ] **Step 6: Commit any fixes from testing**

---

## Post-Phase 2 Notes

After Phase 2 is validated:
- **Phase 3**: Add `att_client.c` — L2CAP framing + ATT protocol (MTU Exchange, Read By Group Type, Read By Type, Find Information). Uses `BleConnMgr_queueTx()` to send ATT requests and reads ATT responses from the RX packet stream.
- **Phase 4**: Add GATT_DISCOVER/GATT_READ/GATT_WRITE commands to command_processor.c and Python API.
- The `BleConnMgr_queueTx()` function added in this phase is the integration point for Phase 3.
