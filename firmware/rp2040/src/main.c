/*
 * FeralRF RP2040 - Main Entry Point
 * Phase 0: USB echo test
 */

#include "config.h"
#include "hardware/gpio.h"
#include "hardware/uart.h"
#include "pico/stdlib.h"
#include <stdio.h>

/* Initialize LEDs */
static void leds_init(void) {
    gpio_init(LED1_PIN);
    gpio_init(LED2_PIN);
    gpio_init(LED3_PIN);
    gpio_set_dir(LED1_PIN, GPIO_OUT);
    gpio_set_dir(LED2_PIN, GPIO_OUT);
    gpio_set_dir(LED3_PIN, GPIO_OUT);
    /* LEDs are active low, start off */
    gpio_put(LED1_PIN, 1);
    gpio_put(LED2_PIN, 1);
    gpio_put(LED3_PIN, 1);
}

/* Initialize CC1352 reset control */
static void reset_init(void) {
    gpio_init(RESET_CC_PIN);
    gpio_set_dir(RESET_CC_PIN, GPIO_OUT);
    gpio_put(RESET_CC_PIN, 1); /* Release reset */
}

/* Initialize UART to CC1352 */
static void uart_cc1352_init(void) {
    uart_init(UART_ID, UART_BAUD_RATE);

    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RTS_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_CTS_PIN, GPIO_FUNC_UART);

    /* Enable flow control */
    uart_set_hw_flow(UART_ID, true, true);

    /* Enable FIFO */
    uart_set_fifo_enabled(UART_ID, true);
}

int main(void) {
    stdio_init_all();

    leds_init();
    reset_init();
    uart_cc1352_init();

    /* Blink LED1 to show we're alive */
    bool led_state = false;

    while (true) {
        /* USB echo: read from stdin, write to stdout */
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            putchar(c);
            /* Also send to UART */
            uart_putc_raw(UART_ID, (char)c);
            /* Toggle LED2 on activity */
            gpio_put(LED2_PIN, 0);
        } else {
            gpio_put(LED2_PIN, 1);
        }

        /* Heartbeat LED */
        static uint32_t last_blink = 0;
        if (time_us_32() - last_blink > 500000) {
            last_blink = time_us_32();
            led_state = !led_state;
            gpio_put(LED1_PIN, led_state ? 0 : 1);
        }
    }

    return 0;
}
