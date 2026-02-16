/*
 * FeralRF CC1352 - Main Entry Point
 * Phase 0: Simple blinky to verify build
 */

#include "config.h"
#include <stdint.h>

/* CC1352P GPIO registers */
#define GPIO_BASE 0x40022000
#define GPIO_DOE31_0 (*(volatile uint32_t *)(GPIO_BASE + 0x200))
#define GPIO_DOUT31_0 (*(volatile uint32_t *)(GPIO_BASE + 0x008))
#define GPIO_DIN31_0 (*(volatile uint32_t *)(GPIO_BASE + 0x010))

/* CC1352P IOC registers */
#define IOC_BASE 0x40081000
#define IOC_PORT_CFG(pin) (*(volatile uint32_t *)(IOC_BASE + 0x000 + (pin) * 4))

/* IOC Port Configuration */
#define IOC_PORT_GPIO 0x00000000
#define IOC_INPUT_ENABLE (1 << 18)
#define IOC_OUTPUT_ENABLE (1 << 18) /* Actually bit 18 is input en, 20 is output en */

/* Simple delay */
static void delay(volatile uint32_t count) {
    while (count--) {
        __asm__ volatile("nop");
    }
}

/* Configure GPIO pin as output */
static void gpio_output_init(uint8_t pin) {
    /* Set IOC to GPIO mode with output enable */
    IOC_PORT_CFG(pin) = IOC_PORT_GPIO | (1 << 20); /* Output enable */

    /* Enable in GPIO DOE */
    GPIO_DOE31_0 |= (1 << pin);
}

/* Set GPIO pin */
static void gpio_set(uint8_t pin, uint8_t value) {
    if (value) {
        GPIO_DOUT31_0 |= (1 << pin);
    } else {
        GPIO_DOUT31_0 &= ~(1 << pin);
    }
}

int main(void) {
    /* Initialize LED pin */
    gpio_output_init(LED_PIN);

    /* Blinky loop */
    while (1) {
        gpio_set(LED_PIN, 1);
        delay(500000);
        gpio_set(LED_PIN, 0);
        delay(500000);
    }

    return 0;
}
