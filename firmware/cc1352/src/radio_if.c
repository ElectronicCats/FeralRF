/*
 * FeralRF CC1352 - Radio Interface (stub)
 */

#include "radio_if.h"

static bool s_rx_running = false;

void RadioIF_init(void) {
    s_rx_running = false;
}

bool RadioIF_startRx(void) {
    s_rx_running = true;
    return true;
}

void RadioIF_stopRx(void) {
    s_rx_running = false;
}

bool RadioIF_isRxRunning(void) {
    return s_rx_running;
}

void RadioIF_poll(void) {
    /* Placeholder for future RF Core event processing. */
}
