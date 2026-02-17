/*
 * FeralRF CC1352 - Output Interface
 *
 * Centralizes host-bound frame construction and enqueue.
 */

#ifndef OUTPUT_IF_H
#define OUTPUT_IF_H

#include <stdint.h>

void OutputIF_sendResponse(uint8_t rsp_cmd, uint8_t seq, const uint8_t *payload,
                           uint16_t payload_len);

#endif /* OUTPUT_IF_H */
