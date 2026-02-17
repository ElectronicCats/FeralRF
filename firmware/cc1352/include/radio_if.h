/*
 * FeralRF CC1352 - Radio Interface
 *
 * Stub implementation to prepare the architecture for RF pipeline integration.
 */

#ifndef RADIO_IF_H
#define RADIO_IF_H

#include <stdbool.h>

void RadioIF_init(void);
bool RadioIF_startRx(void);
void RadioIF_stopRx(void);
bool RadioIF_isRxRunning(void);
void RadioIF_poll(void);

#endif /* RADIO_IF_H */
