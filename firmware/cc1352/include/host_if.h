/*
 * FeralRF CC1352 - Host Interface (UART)
 *
 * Naming aligned with the TI sniffer reference (HostIF_*).
 */

#ifndef HOST_IF_H
#define HOST_IF_H

#include <stddef.h>
#include <stdint.h>

void HostIF_init(void);
void HostIF_writeBuffer(const uint8_t *buffer, size_t len);
void HostIF_writeByte(uint8_t value);
int32_t HostIF_readByteNonBlocking(void);

#endif /* HOST_IF_H */
