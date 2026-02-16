/*
 * FeralRF RP2040 - USB Bridge
 * Phase 0: Stub implementation
 */

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "config.h"

/* USB to UART bridge implementation (Phase 1+) */

void usb_bridge_init(void)
{
    /* TODO: Initialize USB CDC */
}

void usb_bridge_task(void)
{
    /* TODO: Poll USB CDC and bridge to UART */
}
