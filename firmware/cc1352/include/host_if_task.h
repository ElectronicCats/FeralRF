/*
 * FeralRF CC1352 - Host Interface Task (polling variant)
 */

#ifndef HOST_IF_TASK_H
#define HOST_IF_TASK_H

void HostIFTask_init(void);
void HostIFTask_poll(void);
void HostIFTask_processPendingCommand(void);

#endif /* HOST_IF_TASK_H */
