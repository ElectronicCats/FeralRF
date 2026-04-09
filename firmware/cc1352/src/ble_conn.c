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
/* clang-format on */
#include <ti/drivers/rf/RF.h>

/* ── Static state (single connection) ── */
static BleConn_State s_state;
static uint8_t s_ll_data[BLE_CONN_LLDATA_LEN];
static uint16_t s_own_addr_u16[3];  /* 16-bit aligned for RF core */
static uint16_t s_peer_addr_u16[3]; /* 16-bit aligned for RF core */

/* ── Random number helpers ── */
/* Simple xorshift32 PRNG seeded from RF_getCurrentTime().
 * TRNG hangs because PERIPH power domain is not enabled in our TI-RTOS config.
 * For connection parameters, a PRNG seeded from the RAT timer is sufficient. */
static uint32_t s_prng_state;

static uint32_t ble_conn_rand32(void) {
    if (s_prng_state == 0) {
        s_prng_state = RF_getCurrentTime() ^ 0xDEADBEEFu;
    }
    uint32_t x = s_prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_prng_state = x;
    return x;
}

static uint8_t ble_conn_rand_hop(void) {
    /* BLE spec: hop increment must be 5..16 */
    return (uint8_t)(5u + (ble_conn_rand32() % 12u));
}

/* ── LLData builder ── */
static void ble_conn_build_ll_data(uint16_t interval, uint16_t timeout) {
    uint32_t aa = ble_conn_rand32();
    uint32_t crc = ble_conn_rand32() & 0x00FFFFFFu;
    uint8_t hop = ble_conn_rand_hop();
    uint16_t win_offset = (uint16_t)(5u + (ble_conn_rand32() % 11u));

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

void BleConn_init(void) {
    memset(&s_state, 0, sizeof(s_state));
    memset(s_ll_data, 0, sizeof(s_ll_data));

    /* Default own address: random static (matches radio_if default) */
    s_state.ownAddr[0] = 0x01u;
    s_state.ownAddr[1] = 0xEEu;
    s_state.ownAddr[2] = 0xDDu;
    s_state.ownAddr[3] = 0xCCu;
    s_state.ownAddr[4] = 0xBBu;
    s_state.ownAddr[5] = 0xAAu;
}

BleConn_Result BleConn_initiate(const uint8_t *peerAddr, uint8_t peerAddrType,
                                uint16_t connIntervalUnits, uint16_t supervTimeoutUnits) {
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
    Ble5_0_cmdBle5Initiator.channel = 37;
    Ble5_0_cmdBle5Initiator.whitening.init = 0x40 + 37;
    Ble5_0_cmdBle5Initiator.phyMode.mainMode = 0; /* 1M */
    Ble5_0_cmdBle5Initiator.phyMode.coding = 0;

    Ble5_0_cmdBle5Initiator.pParams->pRxQ = NULL; /* set by RadioIF */
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAutoFlushIgnored = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAutoFlushCrcErr = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAutoFlushEmpty = 0;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bIncludeLenByte = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bIncludeCrc = 0;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAppendRssi = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAppendStatus = 1;
    Ble5_0_cmdBle5Initiator.pParams->rxConfig.bAppendTimestamp = 1;

    Ble5_0_cmdBle5Initiator.pParams->initConfig.bUseWhiteList = 0; /* pWhiteList = peer addr */
    Ble5_0_cmdBle5Initiator.pParams->initConfig.bDynamicWinOffset = 1;
    Ble5_0_cmdBle5Initiator.pParams->initConfig.deviceAddrType = 1; /* random */
    Ble5_0_cmdBle5Initiator.pParams->initConfig.peerAddrType = peerAddrType;
    Ble5_0_cmdBle5Initiator.pParams->initConfig.bStrictLenFilter = 1;
    Ble5_0_cmdBle5Initiator.pParams->initConfig.chSel = 1; /* CSA#2 */

    Ble5_0_cmdBle5Initiator.pParams->randomState = 0;
    Ble5_0_cmdBle5Initiator.pParams->connectReqLen = BLE_CONN_LLDATA_LEN;
    Ble5_0_cmdBle5Initiator.pParams->pConnectReqData = s_ll_data;
    Ble5_0_cmdBle5Initiator.pParams->pDeviceAddress = s_own_addr_u16;
    Ble5_0_cmdBle5Initiator.pParams->pWhiteList = (rfc_bleWhiteListEntry_t *)s_peer_addr_u16;

    Ble5_0_cmdBle5Initiator.pParams->maxWaitTimeForAuxCh = 0xFFFFu;

    /* connectTime set by RadioIF_bleInitiate() just before RF_runCmd to avoid
     * stale timestamp after mode-switch delays. */

    /* endTrigger + endTime set by RadioIF_bleInitiate() just before RF_runCmd
     * using TRIG_ABSTIME (TRIG_REL_START doesn't work for initiator pParams). */
    Ble5_0_cmdBle5Initiator.pParams->endTrigger.triggerType = TRIG_ABSTIME;
    Ble5_0_cmdBle5Initiator.pParams->endTime = 0; /* set by RadioIF */
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

    if (result == -1) {
        return BLE_CONN_ERR_TIMEOUT;
    }
    if (result == -2) {
        return BLE_CONN_ERR_NO_SYNC;
    }
    return BLE_CONN_ERR_RF;
}

void BleConn_disconnect(void) {
    if (s_state.initiating) {
        RadioIF_stopRx();
    }

    s_state.connected = false;
    s_state.initiating = false;
    s_state.eventCounter = 0;
}

bool BleConn_isConnected(void) {
    return s_state.connected;
}

bool BleConn_isInitiating(void) {
    return s_state.initiating;
}

const BleConn_State *BleConn_getState(void) {
    return &s_state;
}
