/*
 * FeralRF CC1352 - Main Entry Point
 * Phase 2: UART command processor (COBS + CRC16).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/gpio.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/ioc.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/prcm.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/sys_ctrl.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/systick.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/uart.h>

#include "config.h"
#include "protocol.h"

/* Commands (match python/feralrf/enums.py) */
#define CMD_RADIO_INIT 0x01u
#define CMD_SET_CHANNEL 0x02u
#define CMD_SET_POWER 0x03u
#define CMD_SET_PHY 0x04u
#define CMD_GET_INFO 0x05u
#define CMD_RX_START 0x10u
#define CMD_RX_STOP 0x11u

/* Responses (match python/feralrf/enums.py) */
#define RSP_ACK 0x80u
#define RSP_ERROR 0x81u
#define RSP_INFO 0x94u

/* Error codes */
#define ERR_INVALID_CMD 0x01u
#define ERR_INVALID_PAYLOAD 0x02u
#define ERR_INVALID_FRAME 0x03u
#define ERR_FRAME_TOO_LONG 0x04u

/* Firmware info payload */
#define FW_VERSION_MAJOR 0x01u
#define FW_VERSION_MINOR 0x00u
#define FW_VERSION_PATCH 0x00u
#define FW_CAPABILITIES 0x01u

static uint8_t g_selected_phy = 0;
static uint16_t g_channel = 0;
static int8_t g_tx_power_dbm = 0;
static uint32_t g_frequency_hz = 0;
static bool g_rx_enabled = false;

static const uint8_t g_serial[8] = {'F', 'E', 'R', 'A', 'L', 'R', 'F', '1'};

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

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

static void uart_init(void) {
    IOCPinTypeUart(UART0_BASE, UART_RX_PIN, UART_TX_PIN, IOID_UNUSED, IOID_UNUSED);

    UARTDisable(UART0_BASE);
    UARTConfigSetExpClk(UART0_BASE, SysCtrlClockGet(), UART_BAUD_RATE,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    UARTFIFOEnable(UART0_BASE);
    UARTEnable(UART0_BASE);
}

static void uart_write(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        UARTCharPut(UART0_BASE, data[i]);
    }
}

static void send_response(uint8_t rsp_cmd, uint8_t seq, const uint8_t *payload, uint16_t payload_len) {
    uint8_t raw_frame[PROTOCOL_MAX_FRAME];
    uint8_t encoded[COBS_MAX_ENCODED];

    size_t raw_len = protocol_build_frame(rsp_cmd, seq, payload, payload_len, raw_frame);
    size_t encoded_len = cobs_encode(raw_frame, raw_len, encoded);

    uart_write(encoded, encoded_len);
    UARTCharPut(UART0_BASE, 0x00u);
}

static void send_ack(uint8_t seq) {
    send_response(RSP_ACK, seq, NULL, 0);
}

static void send_error(uint8_t seq, uint8_t error_code) {
    uint8_t payload[1] = {error_code};
    send_response(RSP_ERROR, seq, payload, sizeof(payload));
}

static void send_info(uint8_t seq) {
    uint8_t payload[12];

    payload[0] = FW_VERSION_MAJOR;
    payload[1] = FW_VERSION_MINOR;
    payload[2] = FW_VERSION_PATCH;
    payload[3] = FW_CAPABILITIES;
    for (size_t i = 0; i < sizeof(g_serial); i++) {
        payload[4 + i] = g_serial[i];
    }

    send_response(RSP_INFO, seq, payload, sizeof(payload));
}

static void handle_command(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t payload_len) {
    switch (cmd) {
    case CMD_RADIO_INIT:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        g_rx_enabled = false;
        send_ack(seq);
        return;

    case CMD_GET_INFO:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        send_info(seq);
        return;

    case CMD_SET_PHY:
        if (!(payload_len == 1 || payload_len == 7)) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        g_selected_phy = payload[0];
        if (payload_len >= 3) {
            g_channel = read_u16_le(&payload[1]);
        }
        if (payload_len == 7) {
            g_frequency_hz = read_u32_le(&payload[3]);
        }
        send_ack(seq);
        return;

    case CMD_SET_CHANNEL:
        if (payload_len != 1) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        g_channel = payload[0];
        send_ack(seq);
        return;

    case CMD_SET_POWER:
        if (payload_len != 1) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        g_tx_power_dbm = (int8_t)payload[0];
        send_ack(seq);
        return;

    case CMD_RX_START:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        g_rx_enabled = true;
        send_ack(seq);
        return;

    case CMD_RX_STOP:
        if (payload_len != 0) {
            send_error(seq, ERR_INVALID_PAYLOAD);
            return;
        }
        g_rx_enabled = false;
        send_ack(seq);
        return;

    default:
        send_error(seq, ERR_INVALID_CMD);
        return;
    }
}

static void process_encoded_frame(const uint8_t *encoded_frame, size_t encoded_len) {
    uint8_t frame[PROTOCOL_MAX_FRAME];
    uint8_t payload[PROTOCOL_MAX_PAYLOAD];
    uint8_t cmd = 0;
    uint8_t seq = 0;
    uint16_t payload_len = 0;

    size_t frame_len = cobs_decode(encoded_frame, encoded_len, frame);
    if (frame_len == 0) {
        send_error(0, ERR_INVALID_FRAME);
        return;
    }

    if (!protocol_parse_frame(frame, frame_len, &cmd, &seq, payload, &payload_len)) {
        send_error(seq, ERR_INVALID_FRAME);
        return;
    }

    handle_command(cmd, seq, payload, payload_len);
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
    uart_init();
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

        int32_t ch = UARTCharGetNonBlocking(UART0_BASE);
        if (ch < 0) {
            continue;
        }

        uint8_t byte = (uint8_t)ch;
        if (byte == 0x00u) {
            if (overflow) {
                overflow = false;
                encoded_len = 0;
                send_error(0, ERR_FRAME_TOO_LONG);
                continue;
            }

            if (encoded_len == 0) {
                continue;
            }

            process_encoded_frame(encoded_frame, encoded_len);
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
