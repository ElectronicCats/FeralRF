/*
 * FeralRF CC1352 - Host Interface implementation (UART)
 */

#include "host_if.h"

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include DeviceFamily_constructPath(driverlib/uart.h)
/* clang-format on */

#include "config.h"

void HostIF_init(void) {
    /*
     * Flow control is conditionally enabled.  When the RP2040 bridge firmware
     * also has hw-flow-control configured, define UART_HW_FLOW_CONTROL=1 in
     * config.h to activate CTS/RTS on the CC1352 side.
     */
#if UART_HW_FLOW_CONTROL
    IOCPinTypeUart(UART0_BASE, UART_RX_PIN, UART_TX_PIN, UART_CTS_PIN, UART_RTS_PIN);
#else
    IOCPinTypeUart(UART0_BASE, UART_RX_PIN, UART_TX_PIN, IOID_UNUSED, IOID_UNUSED);
#endif

    UARTDisable(UART0_BASE);
    UARTConfigSetExpClk(UART0_BASE, SysCtrlClockGet(), UART_BAUD_RATE,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    UARTFIFOEnable(UART0_BASE);
#if UART_HW_FLOW_CONTROL
    UARTHwFlowControlEnable(UART0_BASE);
#endif
    UARTEnable(UART0_BASE);
}

void HostIF_writeBuffer(const uint8_t *buffer, size_t len) {
    for (size_t i = 0; i < len; i++) {
        UARTCharPut(UART0_BASE, buffer[i]);
    }
}

int32_t HostIF_readByteNonBlocking(void) {
    return UARTCharGetNonBlocking(UART0_BASE);
}
