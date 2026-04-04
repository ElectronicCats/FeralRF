/*
 * FeralRF — Stubs for TI-RTOS symbols not needed in Phase M1.
 * These will be replaced by real implementations when BLE5-Stack is added (M2).
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* OSAL heap stubs removed — SDK 8.30 provides them in rtos_heaposal.h */

/* POSIX pthread cleanup — needed by Idle list but we don't use pthreads */
void _pthread_cleanupFxn(void) {}

/* ICall_getMaxMSecs — inline in non-JT, needs stub for JT mode */
uint32_t ICall_getMaxMSecs(void) { return 0xFFFFFFFFu; }

/* Newlib syscall stubs */
void _exit(int status) { (void)status; while(1); }
int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
int _getpid(void) { return 1; }

/* ClockSupport timer struct */
#include <ti/sysbios/family/arm/cc26xx/Timer.h>
Timer_Struct ClockSupport_timerStruct;

/* BLE user config stubs — replaced when ti_ble_config.c compiles
 * (currently blocked by SDK header space-in-path bug) */
const void *bleStackConfig = NULL;
const void *driverTable = NULL;
const void *boardConfig = NULL;

typedef struct {
    uint32_t timerTickPeriod;
    uint32_t timerMaxMillisecond;
    const void *assertCback;
    const void *icallServiceTbl;
} applicationService_stub_t;

static applicationService_stub_t s_bleAppServiceInfo = {0, 0, NULL, NULL};
void *bleAppServiceInfoTable = &s_bleAppServiceInfo;
