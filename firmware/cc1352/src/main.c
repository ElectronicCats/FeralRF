/*
 * FeralRF CC1352 - Main Entry Point
 * Phase 0: Simple blinky using DriverLib.
 */

#include <stdint.h>

#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/gpio.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/ioc.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/prcm.h>

#include "config.h"

#define BLINK_DELAY_CYCLES 12000000u

static void delay_cycles(volatile uint32_t cycles) {
    while (cycles--) {
        __asm__ volatile("nop");
    }
}

static void board_gpio_init(void) {
    PRCMPowerDomainOn(PRCM_DOMAIN_PERIPH);
    while (PRCMPowerDomainsAllOn(PRCM_DOMAIN_PERIPH) != PRCM_DOMAIN_POWER_ON) {
        /* Wait until peripheral domain is powered. */
    }

    PRCMPeripheralRunEnable(PRCM_PERIPH_GPIO);
    PRCMLoadSet();
    while (!PRCMLoadGet()) {
        /* Wait until clock settings are applied. */
    }

    IOCPortConfigureSet(LED_PIN, IOC_PORT_GPIO,
                        IOC_CURRENT_8MA | IOC_STRENGTH_MAX | IOC_NO_IOPULL |
                            IOC_SLEW_DISABLE | IOC_HYST_DISABLE | IOC_NO_EDGE |
                            IOC_INT_DISABLE | IOC_IOMODE_NORMAL | IOC_NO_WAKE_UP |
                            IOC_INPUT_DISABLE);
    GPIO_setOutputEnableDio(LED_PIN, GPIO_OUTPUT_ENABLE);
    GPIO_clearDio(LED_PIN);
}

int main(void) {
    board_gpio_init();

    while (1) {
        GPIO_setDio(LED_PIN);
        delay_cycles(BLINK_DELAY_CYCLES);
        GPIO_clearDio(LED_PIN);
        delay_cycles(BLINK_DELAY_CYCLES);
    }
}
