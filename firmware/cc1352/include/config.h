/*
 * FeralRF CC1352 Configuration
 */

#ifndef CONFIG_H
#define CONFIG_H

/* UART Configuration */
#define UART_TX_PIN 12         /* DIO12 */
#define UART_RX_PIN 13         /* DIO13 */
#define UART_RTS_PIN 14        /* DIO14 */
#define UART_CTS_PIN 15        /* DIO15 */
#define UART_BAUD_RATE 3000000 /* 3Mbps */

/* LED Configuration (for debugging) */
#define LED_PIN 24 /* DIO24 - LED on CatSniffer */

/* Radio Configuration */
#define TX_POWER_DBM 0 /* Default TX power */

/* Buffer Sizes */
#define UART_RX_BUFFER_SIZE 16384 /* 16KB */
#define UART_TX_BUFFER_SIZE 4096  /* 4KB */

#endif /* CONFIG_H */
