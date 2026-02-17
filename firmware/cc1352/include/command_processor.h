/*
 * FeralRF CC1352 - Command Processor
 */

#ifndef COMMAND_PROCESSOR_H
#define COMMAND_PROCESSOR_H

#include <stddef.h>
#include <stdint.h>

void CommandProcessor_init(void);
void CommandProcessor_processEncodedFrame(const uint8_t *encoded_frame, size_t encoded_len);
void CommandProcessor_sendFrameTooLongError(void);

#endif /* COMMAND_PROCESSOR_H */
