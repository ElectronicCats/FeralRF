/*
 * FeralRF CC1352 - Main Entry Point
 * Phase 2: infrastructure loop + command processor module.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/gpio.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/ioc.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/prcm.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/sys_ctrl.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/systick.h>

#include "command_processor.h"
#include "config.h"
#include "host_if.h"
#include "protocol.h"

static void board_power_init(void) {
    PRCMPowerDomainOn(PRCM_DOMAIN_PERIPH | PRCM_DOMAIN_SERIAL);
    while (PRCMPowerDomainsAllOn(PRCM_DOMAIN_PERIPH | PRCM_DOMAIN_SERIAL) != PRCM_DOMAIN_POWER_ON) {
        /* Wait until peripheral and serial domains are powered. */
    }

    PRCMPeripheralRunEnable(PRCM_PERIPH_GPIO);
    PRCMPeripheralRunEnable(PRCM_PERIPH_UART0);
    PRCMLoadSet();
    while (!PRCMLoadGet()) {
        /* Wait until clock settings are applied. */
    }
}

static void board_gpio_init(void) {
    IOCPortConfigureSet(LED_PIN, IOC_PORT_GPIO,
                        IOC_CURRENT_8MA | IOC_STRENGTH_MAX | IOC_NO_IOPULL |
                            IOC_SLEW_DISABLE | IOC_HYST_DISABLE | IOC_NO_EDGE |
                            IOC_INT_DISABLE | IOC_IOMODE_NORMAL | IOC_NO_WAKE_UP |
                            IOC_INPUT_DISABLE);
    GPIO_setOutputEnableDio(LED_PIN, GPIO_OUTPUT_ENABLE);
#if LED_ACTIVE_LOW
    GPIO_setDio(LED_PIN);
#else
    GPIO_clearDio(LED_PIN);
#endif
}

static void systick_timebase_init(void) {
    SysTickDisable();
    SysTickPeriodSet(0x00FFFFFFu);
    SysTickEnable();
}

int main(void) {
    uint8_t encoded_frame[COBS_MAX_ENCODED];
    size_t encoded_len = 0;
    bool overflow = false;
    uint32_t systick_last = 0;
    uint32_t systick_cycles_accum = 0;
    uint32_t systick_cycles_per_blink = 0;

    board_power_init();
    board_gpio_init();
    HostIF_init();
    CommandProcessor_init();
    systick_timebase_init();
    systick_last = SysTickValueGet();
    systick_cycles_per_blink = (SysCtrlClockGet() / 1000u) * LED_BLINK_MS;
    if (systick_cycles_per_blink == 0u) {
        systick_cycles_per_blink = 1u;
    }

    while (1) {
        uint32_t systick_now = SysTickValueGet();
        uint32_t elapsed_cycles = 0;

        if (systick_last >= systick_now) {
            elapsed_cycles = systick_last - systick_now;
        } else {
            elapsed_cycles = systick_last + (0x00FFFFFFu - systick_now) + 1u;
        }
        systick_last = systick_now;

        systick_cycles_accum += elapsed_cycles;
        if (systick_cycles_accum >= systick_cycles_per_blink) {
            systick_cycles_accum -= systick_cycles_per_blink;
            GPIO_toggleDio(LED_PIN);
        }

        int32_t ch = HostIF_readByteNonBlocking();
        if (ch < 0) {
            continue;
        }

        uint8_t byte = (uint8_t)ch;
        if (byte == 0x00u) {
            if (overflow) {
                overflow = false;
                encoded_len = 0;
                CommandProcessor_sendFrameTooLongError();
                continue;
            }

            if (encoded_len == 0) {
                continue;
            }

            CommandProcessor_processEncodedFrame(encoded_frame, encoded_len);
            encoded_len = 0;
            continue;
        }

        if (!overflow) {
            if (encoded_len < sizeof(encoded_frame)) {
                encoded_frame[encoded_len++] = byte;
            } else {
                overflow = true;
            }
        }
    }
}
