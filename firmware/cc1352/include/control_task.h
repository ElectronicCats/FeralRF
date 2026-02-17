/*
 * FeralRF CC1352 - Control Task (polling variant)
 */

#ifndef CONTROL_TASK_H
#define CONTROL_TASK_H

#include <stdbool.h>
#include <stdint.h>

void ControlTask_init(void);
void ControlTask_onRadioInit(void);
bool ControlTask_onSetPhy(uint8_t phy, uint16_t channel, uint32_t frequency_hz);
void ControlTask_onSetChannel(uint8_t channel);
void ControlTask_onSetPower(int8_t power_dbm);
void ControlTask_onRxStart(void);
void ControlTask_onRxStop(void);
bool ControlTask_isRxEnabled(void);
void ControlTask_getInfoPayload(uint8_t *payload, uint16_t payload_len);

#endif /* CONTROL_TASK_H */
