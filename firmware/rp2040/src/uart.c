/*
 * FeralRF RP2040 - UART Handler
 * Phase 0: Stub implementation
 */

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "config.h"

/* UART buffer and interrupt handling (Phase 1+) */

void uart_handler_init(void)
{
    /* TODO: Initialize UART interrupt handler */
}

void uart_send_buffer(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uart_putc_raw(UART_ID, data[i]);
    }
}

uint32_t uart_read_available(void)
{
    return uart_is_readable(UART_ID) ? 1 : 0;
}

int uart_read_byte(void)
{
    if (uart_is_readable(UART_ID)) {
        return uart_getc(UART_ID);
    }
    return -1;
}
