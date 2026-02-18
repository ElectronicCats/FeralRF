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
bool ControlTask_onTxRaw(const uint8_t *payload, uint8_t payload_len, int8_t power_dbm);
bool ControlTask_onTxFrame(const uint8_t *payload, uint8_t payload_len);
bool ControlTask_onTxBurst(const uint8_t *payload, uint8_t payload_len, uint16_t count,
                           uint32_t interval_us);
bool ControlTask_onTxContinuous(const uint8_t *payload, uint8_t payload_len, uint32_t interval_us);
void ControlTask_onTxStop(void);
void ControlTask_processTxRaw(void);
void ControlTask_processTxBurst(void);
void ControlTask_processTxContinuous(void);
bool ControlTask_isTxBurstPending(void);
bool ControlTask_isTxContinuousPending(void);
bool ControlTask_isTxBusy(void);
void ControlTask_onRxStart(void);
void ControlTask_onRxStop(void);
bool ControlTask_isRxEnabled(void);
void ControlTask_getInfoPayload(uint8_t *payload, uint16_t payload_len);

#endif /* CONTROL_TASK_H */
