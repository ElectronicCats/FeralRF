/*
 * FeralRF CC1352 - Protocol Implementation (COBS + CRC16)
 */

#include "protocol.h"

/*
 * CRC-16-CCITT Implementation
 * Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
 * Initial value: 0xFFFF
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/*
 * COBS (Consistent Overhead Byte Stuffing) Encode
 *
 * Encodes data by replacing all 0x00 bytes with a length code,
 * allowing 0x00 to be used as a frame delimiter.
 *
 * Reference: https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing
 */
size_t cobs_encode(const uint8_t *input, size_t input_len, uint8_t *output) {
    if (input_len == 0) {
        return 0;
    }

    size_t read_idx = 0;
    size_t write_idx = 1;
    size_t code_idx = 0;
    uint8_t code = 1;

    while (read_idx < input_len) {
        if (input[read_idx] == 0x00) {
            /* Found a zero - write the code and start new block */
            output[code_idx] = code;
            code_idx = write_idx++;
            code = 1;
            read_idx++;
        } else {
            /* Copy non-zero byte */
            output[write_idx++] = input[read_idx++];

            if (++code == 0xFF) {
                /* Code reached max value - emit and reset */
                output[code_idx] = code;
                code_idx = write_idx++;
                code = 1;
            }
        }
    }

    /* Write final code */
    output[code_idx] = code;

    return write_idx;
}

/*
 * COBS Decode
 *
 * Decodes COBS-encoded data back to original form.
 * Returns 0 on error (invalid encoding).
 */
size_t cobs_decode(const uint8_t *input, size_t input_len, uint8_t *output) {
    if (input_len == 0) {
        return 0;
    }

    size_t read_idx = 0;
    size_t write_idx = 0;

    while (read_idx < input_len) {
        uint8_t code = input[read_idx++];

        if (code == 0) {
            /* Invalid: code byte cannot be 0 in COBS */
            return 0;
        }

        if (read_idx + code - 1 > input_len) {
            /* Invalid: not enough input data */
            return 0;
        }

        /* Copy (code - 1) bytes */
        for (uint8_t i = 1; i < code; i++) {
            output[write_idx++] = input[read_idx++];
        }

        /* Add zero byte (unless at end or code was 0xFF) */
        if (code != 0xFF && read_idx < input_len) {
            output[write_idx++] = 0x00;
        }
    }

    return write_idx;
}

/*
 * Build a complete frame with CRC
 *
 * Frame format: [CMD_ID:1][SEQ:1][LEN:2][PAYLOAD:N][CRC16:2]
 */
size_t protocol_build_frame(uint8_t cmd_id, uint8_t seq, const uint8_t *payload, size_t payload_len,
                            uint8_t *output) {
    size_t idx = 0;

    /* Header */
    output[idx++] = cmd_id;
    output[idx++] = seq;
    output[idx++] = payload_len & 0xFF;        /* LEN low byte */
    output[idx++] = (payload_len >> 8) & 0xFF; /* LEN high byte */

    /* Payload */
    if (payload != NULL && payload_len > 0) {
        for (size_t i = 0; i < payload_len; i++) {
            output[idx++] = payload[i];
        }
    }

    /* CRC16 over header + payload */
    uint16_t crc = crc16_ccitt(output, idx);
    output[idx++] = crc & 0xFF;        /* CRC low byte */
    output[idx++] = (crc >> 8) & 0xFF; /* CRC high byte */

    return idx;
}

/*
 * Parse a frame and verify CRC
 */
bool protocol_parse_frame(const uint8_t *frame, size_t frame_len, uint8_t *cmd_id, uint8_t *seq,
                          uint8_t *payload, uint16_t *payload_len) {
    /* Minimum frame: CMD(1) + SEQ(1) + LEN(2) + CRC(2) = 6 bytes */
    if (frame_len < PROTOCOL_OVERHEAD) {
        return false;
    }

    /* Parse header */
    *cmd_id = frame[0];
    *seq = frame[1];
    *payload_len = frame[2] | ((uint16_t)frame[3] << 8);

    /* Verify total length */
    if (frame_len != (size_t)(PROTOCOL_OVERHEAD + *payload_len)) {
        return false;
    }

    /* Verify CRC */
    uint16_t frame_crc = frame[frame_len - 2] | ((uint16_t)frame[frame_len - 1] << 8);
    uint16_t calc_crc = crc16_ccitt(frame, frame_len - 2);

    if (frame_crc != calc_crc) {
        return false;
    }

    /* Copy payload */
    if (*payload_len > 0 && payload != NULL) {
        for (size_t i = 0; i < *payload_len; i++) {
            payload[i] = frame[4 + i];
        }
    }

    return true;
}
