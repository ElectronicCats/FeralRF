/*
 * FeralRF — Stubs for TI-RTOS symbols
 *
 * Fase 0.0: minimal stubs for kernel + power driver
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ─── Power driver config (required by PowerCC26X2.c) ─── */

#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26X2.h>
#include <ti/drivers/temperature/TemperatureCC26X2.h>

const PowerCC26X2_Config PowerCC26X2_config = {
    .policyInitFxn = NULL,
    .policyFxn = NULL,
    .calibrateFxn = &PowerCC26XX_calibrate,
    .enablePolicy = false,
    .calibrateRCOSC_LF = true,
    .calibrateRCOSC_HF = true,
    .enableTCXOFxn = NULL,
};

const TemperatureCC26X2_Config TemperatureCC26X2_config = {
    .intPriority = (uint8_t)~0,
};

/* ─── OSAL heap stubs (required by HeapCallback.c) ─── */

void *osalHeapAllocFxn(void *heap, size_t size, size_t alignment) {
    (void)heap; (void)size; (void)alignment;
    return NULL;
}

void osalHeapFreeFxn(void *heap, void *ptr, size_t size) {
    (void)heap; (void)ptr; (void)size;
}

void osalHeapGetStatsFxn(void *heap, void *stats) {
    (void)heap; (void)stats;
}

bool osalHeapIsBlockingFxn(void *heap) {
    (void)heap;
    return false;
}

void osalHeapInitFxn(void *heap, void *buf, size_t size) {
    (void)heap; (void)buf; (void)size;
}

/* ─── POSIX pthread cleanup ─── */

void _pthread_cleanupFxn(void) {}

/* ─── Newlib syscall stubs ─── */

void _exit(int status) { (void)status; while(1); }
int _kill(int pid, int sig) { (void)pid; (void)sig; return -1; }
int _getpid(void) { return 1; }

/* ─── ClockSupport timer struct ─── */

#include <ti/sysbios/family/arm/cc26xx/Timer.h>
Timer_Struct ClockSupport_timerStruct;
