/*
 * FeralRF CC1352 - Protocol (COBS + CRC16)
 *
 * Frame format (pre-COBS):
 * [CMD_ID:1][SEQ:1][LEN:2][PAYLOAD:N][CRC16:2]
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Maximum payload size */
#define PROTOCOL_MAX_PAYLOAD 255

/* Frame overhead: CMD(1) + SEQ(1) + LEN(2) + CRC(2) = 6 bytes */
#define PROTOCOL_OVERHEAD 6

/* Maximum frame size (pre-COBS) */
#define PROTOCOL_MAX_FRAME (PROTOCOL_MAX_PAYLOAD + PROTOCOL_OVERHEAD)

/* COBS overhead: worst case is (N/254 + 1) extra bytes */
#define COBS_MAX_OVERHEAD ((PROTOCOL_MAX_FRAME / 254) + 1)

/* Maximum COBS-encoded frame size */
#define COBS_MAX_ENCODED (PROTOCOL_MAX_FRAME + COBS_MAX_OVERHEAD + 1)

/* -------------------------------------------------------------------------
 * Command IDs
 * ------------------------------------------------------------------------- */

/* Crypto HW (F25) */
#define CMD_RANDOM 0x59
#define CMD_AES_ECB 0x5A
#define CMD_AES_CCM 0x5B
#define CMD_AES_CTR 0x5C
#define CMD_AES_CBC 0x5D
#define CMD_AES_GCM 0x5E
#define CMD_SHA256 0x5F
#define CMD_ECDH 0x60
#define CMD_ECDSA_SIGN 0x61
#define CMD_ECDSA_VERIFY 0x62

/* Crypto responses */
#define RSP_RANDOM 0x95
#define RSP_AES 0x96
#define RSP_AES_CCM 0x97
#define RSP_AES_GCM 0x98
#define RSP_SHA256 0x99
#define RSP_ECDH 0x9A
#define RSP_ECDSA_SIG 0x9B
#define RSP_ECDSA_VERIFY 0x9C

/* -------------------------------------------------------------------------
 * Command IDs (consolidated F8f #10) — canonical source of truth.
 * Mirror in python/feralrf/enums.py (Command).
 * ------------------------------------------------------------------------- */

/* Core radio control */
#define CMD_RADIO_INIT 0x01u
#define CMD_SET_CHANNEL 0x02u
#define CMD_SET_POWER 0x03u
#define CMD_SET_PHY 0x04u
#define CMD_GET_INFO 0x05u
#define CMD_GET_STATS 0x06u
#define CMD_SET_ADV_HOP 0x07u
#define CMD_SET_PROP_CONFIG 0x08u
#define CMD_SET_BLE_ADDR 0x09u
#define CMD_SET_BLE_SCAN_MODE 0x0Bu

/* RX */
#define CMD_RX_START 0x10u
#define CMD_RX_STOP 0x11u

/* TX */
#define CMD_TX_RAW 0x20u
#define CMD_TX_CONTINUOUS 0x21u
#define CMD_TX_BURST 0x22u
#define CMD_TX_FRAME 0x23u
#define CMD_TX_STOP 0x24u

/* Jamming */
#define CMD_JAM_CONTINUOUS 0x30u
#define CMD_JAM_STOP 0x33u

/* BLE Connection */
#define CMD_CONNECT 0x40u
#define CMD_DISCONNECT 0x41u
#define CMD_CONN_STATUS 0x42u

/* GATT */
#define CMD_GATT_DISCOVER 0x43u
#define CMD_GATT_SUBSCRIBE 0x44u
#define CMD_GATT_READ 0x45u
#define CMD_GATT_WRITE 0x46u
#define CMD_GATT_EXCHANGE_MTU 0x4Au
#define CMD_GATT_READ_BY_UUID 0x4Bu

/* Diagnostics */
#define CMD_DEBUG_TIMING 0x47u
#define CMD_DEBUG_CONN_PARAMS 0x48u
#define CMD_ATT_DEBUG 0x49u

/* F8b Track B follower */
#define CMD_FOLLOW_START 0x50u
#define CMD_FOLLOW_STOP 0x51u
#define CMD_BLE_ADV_LEGACY 0x52u
#define CMD_FOLLOW_DEBUG 0x54u

/* Test modes (F22) */
#define CMD_TX_CW 0x55u
#define CMD_TX_PRBS 0x56u
#define CMD_TX_TEST_STOP 0x57u

/* Crypto HW (F25) — already defined above as 0x59-0x62 */

/* -------------------------------------------------------------------------
 * Response IDs (consolidated F8f #10) — canonical source of truth.
 * Mirror in python/feralrf/enums.py (Response).
 * ------------------------------------------------------------------------- */

#define RSP_ACK 0x80u
#define RSP_ERROR 0x81u
#define RSP_RX_PACKET 0x90u
#define RSP_STATS 0x93u
#define RSP_INFO 0x94u

/* Crypto HW (F25) — already defined above as 0x95-0x9C */

/* BLE Connection */
#define RSP_CONN_RESULT 0xA0u
#define RSP_CONN_STATUS_R 0xA1u

/* GATT */
#define RSP_GATT_SERVICE 0xA2u
#define RSP_GATT_CHAR 0xA3u
#define RSP_GATT_READ_R 0xA4u
#define RSP_GATT_DONE 0xA5u
#define RSP_GATT_NOTIFY 0xA6u

/* Diagnostics */
#define RSP_DEBUG_TIMING 0xA8u
#define RSP_DEBUG_CONN_PARAMS 0xA9u
#define RSP_ATT_DEBUG 0xAAu

/* F8b Track B */
#define RSP_LL_PACKET 0xABu
#define RSP_FOLLOW_DONE 0xACu
#define RSP_FOLLOW_DEBUG 0xAFu

/* F8c — MTU + Read by UUID + Disconnect reason */
#define RSP_GATT_MTU 0xB0u
#define RSP_GATT_ATTRIBUTE 0xB1u
#define RSP_DISCONNECTED 0xB2u

/* -------------------------------------------------------------------------
 * Error codes (consolidated F8f #10).
 * ------------------------------------------------------------------------- */

#define ERR_INVALID_CMD 0x01u
#define ERR_INVALID_PAYLOAD 0x02u
#define ERR_INVALID_FRAME 0x03u
#define ERR_FRAME_TOO_LONG 0x04u
#define ERR_INVALID_STATE 0x05u
#define ERR_RF_INIT_FAILED 0x06u
#define ERR_RF_NOT_READY 0x07u

/**
 * Calculate CRC-16-CCITT
 * Polynomial: 0x1021, Initial: 0xFFFF
 *
 * @param data  Input data
 * @param len   Length of data
 * @return      CRC16 value
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len);

/**
 * Encode data using COBS
 *
 * @param input     Input data
 * @param input_len Length of input
 * @param output    Output buffer (must be large enough for COBS_MAX_ENCODED)
 * @return          Length of encoded data (without trailing 0x00)
 */
size_t cobs_encode(const uint8_t *input, size_t input_len, uint8_t *output);

/**
 * Decode COBS-encoded data
 *
 * @param input     COBS-encoded input (without trailing 0x00)
 * @param input_len Length of encoded input
 * @param output    Output buffer (must be large enough for original data)
 * @return          Length of decoded data, or 0 on error
 */
size_t cobs_decode(const uint8_t *input, size_t input_len, uint8_t *output);

/**
 * Build a complete frame with CRC
 *
 * @param cmd_id    Command ID
 * @param seq       Sequence number
 * @param payload   Payload data (can be NULL)
 * @param payload_len Length of payload
 * @param output    Output buffer for complete frame
 * @return          Length of frame
 */
size_t protocol_build_frame(uint8_t cmd_id, uint8_t seq, const uint8_t *payload, size_t payload_len,
                            uint8_t *output);

/**
 * Parse a frame and verify CRC
 *
 * @param frame     Frame data (without COBS encoding)
 * @param
 * frame_len Length of frame
 * @param cmd_id    Output: command ID
 * @param seq       Output:
 * sequence number
 * @param payload   Output: payload buffer
 * @param payload_len Output: payload
 * length
 * @return          true on success, false on error
 */
bool protocol_parse_frame(const uint8_t *frame, size_t frame_len, uint8_t *cmd_id, uint8_t *seq,
                          uint8_t *payload, uint16_t *payload_len);

#endif /* PROTOCOL_H */
