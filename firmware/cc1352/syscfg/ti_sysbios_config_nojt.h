/*
 * Minimal config for ICall source compilation (NO ICALL_JT).
 * ICall source files implement the non-JT API and must not have ICALL_JT defined.
 */
#ifndef ti_sysbios_config_nojt_h
#define ti_sysbios_config_nojt_h

/* Device family only — no ICALL_JT, no BLE defines */
#ifndef DeviceFamily_CC13X2X7
#define DeviceFamily_CC13X2X7
#endif

/* RTOS config needed by ICall */
#include "ti_sysbios_config.h"

/* Undo ICALL_JT if it was set by the above include */
#ifdef ICALL_JT
#undef ICALL_JT
#endif

#endif
