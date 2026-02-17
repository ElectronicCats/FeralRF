/*
 * FeralRF CC1352 - Host Interface implementation (UART)
 */

#include "host_if.h"

#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/ioc.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/sys_ctrl.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/uart.h>

#include "config.h"

void HostIF_init(void) {
    IOCPinTypeUart(UART0_BASE, UART_RX_PIN, UART_TX_PIN, IOID_UNUSED, IOID_UNUSED);

    UARTDisable(UART0_BASE);
    UARTConfigSetExpClk(UART0_BASE, SysCtrlClockGet(), UART_BAUD_RATE,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    UARTFIFOEnable(UART0_BASE);
    UARTEnable(UART0_BASE);
}

void HostIF_writeBuffer(const uint8_t *buffer, size_t len) {
    for (size_t i = 0; i < len; i++) {
        UARTCharPut(UART0_BASE, buffer[i]);
    }
}

void HostIF_writeByte(uint8_t value) {
    UARTCharPut(UART0_BASE, value);
}

int32_t HostIF_readByteNonBlocking(void) {
    return UARTCharGetNonBlocking(UART0_BASE);
}
