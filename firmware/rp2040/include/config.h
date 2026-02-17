/*
 * FeralRF RP2040 Configuration
 */

#ifndef CONFIG_H
#define CONFIG_H

/* UART Configuration (to CC1352) */
#define UART_TX_PIN 0          /* GPIO0 */
#define UART_RX_PIN 1          /* GPIO1 */
/* CatSniffer v3 does not provide dedicated RTS/CTS lines to CC1352 UART. */
#define UART_RTS_PIN 2         /* GPIO2 (wired to CC1352 DIO15 / BOOT net) */
#define UART_CTS_PIN 3         /* GPIO3 (shared with RESET_CC net) */
#define UART_BAUD_RATE 3000000 /* 3Mbps */
#define UART_ID uart0

/* Control Signals */
#define RESET_CC_PIN 15 /* GPIO15 - CC1352 Reset */

/* LED Configuration */
#define LED1_PIN 28 /* GPIO28 */
#define LED2_PIN 27 /* GPIO27 */
#define LED3_PIN 26 /* GPIO26 */

/* Buffer Sizes */
#define UART_RX_BUFFER_SIZE 16384 /* 16KB */
#define USB_TX_BUFFER_SIZE 4096   /* 4KB */

#endif /* CONFIG_H */
