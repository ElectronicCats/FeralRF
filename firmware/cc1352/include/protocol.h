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
