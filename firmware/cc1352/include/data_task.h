/*
 * FeralRF CC1352 - Data Task (polling variant)
 */

#ifndef DATA_TASK_H
#define DATA_TASK_H

#include <stdbool.h>

void DataTask_init(void);
void DataTask_poll(void);
bool DataTask_isRxActive(void);

#endif /* DATA_TASK_H */
