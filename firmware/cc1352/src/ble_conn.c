/*
 * FeralRF CC1352 - BLE Connection Manager
 *
 * Generates CONNECT_IND parameters and configures CMD_BLE5_INITIATOR.
 * Reference: Sniffle RadioWrapper_initiate() (RadioWrapper.c:645-737)
 *            Sniffle sniffle_hw.py:initiate_conn() (L512-537)
 */

#include "ble_conn.h"
#include "ble_conn_mgr.h"
#include "ble_conn_pdu.h"
#include "radio_if.h"
#include "smartrf_ble5_0.h"

#include <string.h>

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_ble_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_ble_mailbox.h)
/* clang-format on */
#include <ti/drivers/rf/RF.h>
#include <ti/sysbios/knl/Task.h>

#include "csa2.h"
#include "tx_queue.h"

/* LL Control PDU opcodes used during teardown.
 * Core Spec Vol 6, Part B, 2.4.2 — opcode + ErrorCode payload. */
#define LL_TERMINATE_IND_OPCODE 0x02u
#define LL_TERMINATE_REASON_REMOTE_USER 0x13u

/* TI-RTOS Clock tick = 10 µs. Convert milliseconds to ticks. */
#define MS_TO_TASK_TICKS(ms) ((uint32_t)(ms) * 100u)

/* F1 fix: bounded timeout for CMD_BLE5_INITIATOR. The host's default
 * Python-side timeout for ble_connect is 10 s; firmware terminates
 * 2 s earlier so the host always receives a clean RSP_CONN_RESULT
 * (with BLE_CONN_ERR_TIMEOUT) instead of a host-side TimeoutError.
 * 8 s × 4 MHz RAT = 32_000_000 ticks. */
#define BLE_CONNECT_TIMEOUT_RAT_TICKS (8u * 4000000u)

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

/* ── LLData builder ──
 * Byte-level packing delegated to BleConnPdu_buildLlData (validated by
 * Python contract test python/tests/test_connect_ind_pdu.py). This
 * function still owns the *value* choice (random AA/CRC, all 37 data
 * channels enabled, latency 0) and mirrors the chosen values into
 * BleConn_State.
 */
static void ble_conn_build_ll_data(uint16_t interval, uint16_t timeout) {
    BleConnIndFields fields = {
        .initAddr = {0},
        .initAddrRandom = true,
        .advAddr = {0},
        .advAddrRandom = false,
        .accessAddr = ble_conn_rand32(),
        .crcInit = ble_conn_rand32() & 0x00FFFFFFu,
        .winSize = 3u,
        .winOffset = (uint16_t)(5u + (ble_conn_rand32() % 11u)),
        .interval = interval,
        .latency = 0u,
        .timeout = timeout,
        .channelMap = {0xFFu, 0xFFu, 0xFFu, 0xFFu, 0x1Fu},
        .hopIncrement = ble_conn_rand_hop(),
        .sca = 0u,
    };

    BleConnPdu_buildLlData(&fields, s_ll_data);

    s_state.accessAddr = fields.accessAddr;
    s_state.crcInit = fields.crcInit;
    memcpy(s_state.channelMap, fields.channelMap, 5);
    s_state.hopIncrement = fields.hopIncrement;
    s_state.winOffset = fields.winOffset;
    s_state.connInterval = interval;
    s_state.supervTimeout = timeout;
    s_state.peripheralLatency = 0;
    s_state.eventCounter = 0;
}

/* ── Public API ── */

void BleConn_init(void) {
    memset(&s_state, 0, sizeof(s_state));
    memset(s_ll_data, 0, sizeof(s_ll_data));

    /* Default own address: random static. The MSB (octet 5 in wire/LE order)
     * MUST have its top 2 bits set to 0b11 — otherwise the address subtype is
     * Reserved (0b10) / NRPA (0b00) / RPA (0b01), and a peer applying spec-
     * compliant validation (e.g. CH573) silently drops our CONNECT_IND.
     * BLE 5.0 Vol 6 Part B §1.3.2.1. Sniffle capture on 2026-04-24 confirmed
     * the previous 0xAA value (top bits = 10 = RFU) caused every master event
     * to NOSYNC: the peer never accepted the connection. */
    s_state.ownAddr[0] = 0x01u;
    s_state.ownAddr[1] = 0xEEu;
    s_state.ownAddr[2] = 0xDDu;
    s_state.ownAddr[3] = 0xCCu;
    s_state.ownAddr[4] = 0xBBu;
    s_state.ownAddr[5] = 0xCAu; /* was 0xAA — top 2 bits now 11 (static random) */

    BleConnMgr_init();
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
    Ble5_0_cmdBle5Initiator.phyMode.coding = 4;   /* Sniffle parity for 1M */

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
    Ble5_0_cmdBle5Initiator.pParams->initConfig.chSel =
        0; /* CSA#1 — CH573/BLE 4.2 peers ignore CSA#2 */

    Ble5_0_cmdBle5Initiator.pParams->randomState = 0;
    Ble5_0_cmdBle5Initiator.pParams->connectReqLen = BLE_CONN_LLDATA_LEN;
    Ble5_0_cmdBle5Initiator.pParams->pConnectReqData = s_ll_data;
    Ble5_0_cmdBle5Initiator.pParams->pDeviceAddress = s_own_addr_u16;
    Ble5_0_cmdBle5Initiator.pParams->pWhiteList = (rfc_bleWhiteListEntry_t *)s_peer_addr_u16;

    Ble5_0_cmdBle5Initiator.pParams->maxWaitTimeForAuxCh = 0xFFFFu;

    /* connectTime set by RadioIF_bleInitiate() just before RF_runCmd to avoid
     * stale timestamp after mode-switch delays. */

    /* F1 fix (was: TRIG_NEVER + endTime=0 + TRIG_NEVER): bounded timeout
     * via TRIG_REL_START so RF_runCmd terminates deterministically when
     * the peer never responds. Without this the RF task hangs forever and
     * subsequent commands fail with async ERR_INVALID_STATE — a full
     * reflash is the only recovery (observed against Sony WH-CH720N
     * during F8c live smoke 2026-05-02).
     *
     * The earlier note about a 5-second TRIG_ABSTIME conflicting with
     * bDynamicWinOffset's calibration window referred to ABSTIME
     * specifically; TRIG_REL_START is relative to the command's own
     * start tick and does not race with WinOffset calibration.
     *
     * BLE_DONE_ENDED is the status returned by RF_runCmd when this
     * trigger fires, and RadioIF_bleInitiate already maps it to return
     * value -1, which BleConn_initiate maps to BLE_CONN_ERR_TIMEOUT. */
    Ble5_0_cmdBle5Initiator.pParams->endTrigger.triggerType = TRIG_REL_START;
    Ble5_0_cmdBle5Initiator.pParams->endTime = BLE_CONNECT_TIMEOUT_RAT_TICKS;
    Ble5_0_cmdBle5Initiator.pParams->timeoutTrigger.triggerType = TRIG_NEVER;
    Ble5_0_cmdBle5Initiator.pParams->timeoutTime = 0;

    /* Run the initiator command via RadioIF */
    int result = RadioIF_bleInitiate();

    s_state.initiating = false;

    if (result >= 0) {
        s_state.connected = true;
        /* TI's BLE_DONE_CONNECT only confirms WE sent ChSel=1 — it doesn't say
         * the peer accepted CSA#2. Peers running BLE <5.0 (e.g. CH573) ignore
         * ChSel and follow the legacy hop. Without an over-the-air negotiation
         * primitive in CONNECT_IND we cannot tell the two cases apart, so the
         * firmware defaults to legacy hop. CSA#2 will need explicit feature
         * exchange (LL_FEATURE_REQ → response with bit 27 set) before we flip
         * useCsa2 on. */
        s_state.useCsa2 = false;
        s_state.connTime = Ble5_0_cmdBle5Initiator.pParams->connectTime;

        /* The SDK can rewrite fields in s_ll_data during CMD_BLE5_INITIATOR
         * (Session 3 confirmed bDynamicWinOffset=1 rewrites WinOffset; the
         * Session 5 wire capture showed WinSize was rewritten too). What
         * actually goes on air is whatever ends up in s_ll_data after
         * initiator returns. The slave decodes those values and from event 0
         * computes its listen channel / accepts our AA / verifies our CRC
         * with EXACTLY those bytes. If we keep the build_ll_data values in
         * s_state and feed them to CMD_BLE5_MASTER for TX, our master uses
         * pre-SDK values while the slave is using post-SDK values — every
         * data-channel TX lands on the wrong AA/channel/CRC and the slave
         * silently drops it. NOSYNC every event with nTx==1 (Session 4
         * Task 2 evidence). Re-snapshot ALL fields the host loop consumes. */
        s_state.accessAddr = (uint32_t)s_ll_data[0] | ((uint32_t)s_ll_data[1] << 8) |
                             ((uint32_t)s_ll_data[2] << 16) | ((uint32_t)s_ll_data[3] << 24);
        s_state.crcInit =
            (uint32_t)s_ll_data[4] | ((uint32_t)s_ll_data[5] << 8) | ((uint32_t)s_ll_data[6] << 16);
        /* s_ll_data[7] = WinSize, [8..9] = WinOffset, [10..11] = Interval,
         * [12..13] = Latency, [14..15] = Timeout, [16..20] = ChM, [21] = Hop|SCA. */
        s_state.winOffset = (uint16_t)((uint16_t)s_ll_data[8] | ((uint16_t)s_ll_data[9] << 8));
        memcpy(s_state.channelMap, &s_ll_data[16], 5);
        s_state.hopIncrement = s_ll_data[21] & 0x1Fu;

        BleConnMgr_start();
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
    /* If we have an active connection, send LL_TERMINATE_IND so the peer
     * tears down its side immediately instead of waiting for
     * supervisionTimeout (~1 s for default config). Without this, CH573 and
     * similar peripherals hold ghost connection state long enough to make
     * back-to-back demo runs flaky. Block ~3 connection intervals after
     * queuing so BleConnMgr_poll has time to TX it and receive the slave's
     * empty-PDU ACK before we tear down our own side. */
    if (s_state.connected && BleConnMgr_isRunning()) {
        uint8_t pdu[2] = {LL_TERMINATE_IND_OPCODE, LL_TERMINATE_REASON_REMOTE_USER};
        TXQueue_insert(2, TX_QUEUE_LLID_CTRL, pdu);

        uint32_t interval_ms = (uint32_t)s_state.connInterval * 5u / 4u; /* 1.25 ms units */
        if (interval_ms == 0u) {
            interval_ms = 30u;
        }
        Task_sleep(MS_TO_TASK_TICKS(interval_ms * 3u));
    }

    BleConnMgr_stop();
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

const uint8_t *BleConn_getLlData(void) {
    return s_ll_data;
}
