/*
 * FeralRF — Stubs for TI-RTOS symbols
 *
 * Fase 0.0: minimal stubs for kernel + power driver
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* PowerCC26X2_config and TemperatureCC26X2_config now live in ti_rf_config_min.c */

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
