/*
 * FeralRF CC1352 — TI-RTOS Main Entry Point
 *
 * Sniffle-style architecture:
 * - Single RF task that blocks on RF_runCmd(RX) with callback
 * - UART polling happens between RF_runCmd calls
 * - When a command arrives, RF_cancelCmd stops RX, command is processed,
 *   then RX restarts
 * - RF_open happens ONCE at boot and the handle stays open forever
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* TI-RTOS */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>

/* TI Drivers */
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>

/* Device */
#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/gpio.h)
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/prcm.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include DeviceFamily_constructPath(driverlib/vims.h)
/* clang-format on */

/* BLE5-Stack (Phase M3) */
#define ICALL_JT
#define ICALL_LITE
#define CC13XX
#define CC13X2P
#define SYSCFG
#include "ble_user_config.h"

/* FeralRF app */
#include "command_processor.h"
#include "config.h"
#include "control_task.h"
#include "data_task.h"
#include "host_if.h"
#include "host_if_task.h"
#include "task_event.h"

/* ─── BLE User Config ─── */
#ifndef USE_DEFAULT_USER_CFG
icall_userCfg_t user0Cfg = BLE_USER_CFG;
#endif

/* ─── Task Configuration ─── */

#define MAIN_TASK_PRIORITY   3 /* Same as Sniffle — cooperative with RF SWIs */
#define MAIN_TASK_STACK_SIZE 4096

static Task_Struct s_main_task;
static uint8_t s_main_task_stack[MAIN_TASK_STACK_SIZE];

/* Semaphore for RF callback → main task notification */
Semaphore_Struct s_rf_sem_struct;
Semaphore_Handle g_rf_semaphore = NULL;

/* Unused but kept for compatibility (tx-done semaphore) */
Semaphore_Struct s_tx_done_sem_struct;
Semaphore_Handle g_tx_done_semaphore = NULL;

/* ─── Board Init ─── */

static void board_power_init(void) {
    PRCMPowerDomainOn(PRCM_DOMAIN_PERIPH | PRCM_DOMAIN_SERIAL);
    while (PRCMPowerDomainsAllOn(PRCM_DOMAIN_PERIPH | PRCM_DOMAIN_SERIAL) !=
           PRCM_DOMAIN_POWER_ON) {
    }
    PRCMPeripheralRunEnable(PRCM_PERIPH_GPIO);
    PRCMPeripheralRunEnable(PRCM_PERIPH_UART0);
    PRCMLoadSet();
    while (!PRCMLoadGet()) {
    }
}

static void board_gpio_init(void) {
    IOCPortConfigureSet(LED_PIN, IOC_PORT_GPIO,
                        IOC_CURRENT_8MA | IOC_STRENGTH_MAX | IOC_NO_IOPULL |
                            IOC_SLEW_DISABLE | IOC_HYST_DISABLE | IOC_NO_EDGE |
                            IOC_INT_DISABLE | IOC_IOMODE_NORMAL |
                            IOC_NO_WAKE_UP | IOC_INPUT_DISABLE);
    GPIO_setOutputEnableDio(LED_PIN, GPIO_OUTPUT_ENABLE);
#if LED_ACTIVE_LOW
    GPIO_setDio(LED_PIN);
#else
    GPIO_clearDio(LED_PIN);
#endif
}

/* ─── Main Task: everything in one task (Sniffle pattern) ─── */

static void MainTask_taskFxn(UArg a0, UArg a1) {
    (void)a0;
    (void)a1;

    /* Initialize ALL subsystems */
    HostIF_init();
    TaskEvent_init();
    ControlTask_init();
    CommandProcessor_init();
    HostIFTask_init();
    DataTask_init();

    /* Main loop: UART polling + RF event processing.
     * Like Sniffle, everything happens in one task context.
     * RF_runCmd/RF_postCmd/RF_pendCmd all work here because
     * RF SWIs run at higher priority and can preempt. */
    uint32_t led_counter = 0;
    while (1) {
        /* 1. Poll UART — read bytes, parse commands, send responses */
        HostIFTask_poll();

        /* 2. Process RF events — drain RX packets, handle deferred ops */
        DataTask_poll();

        /* 3. LED blink */
        led_counter++;
        if (led_counter >= 50000u) {
            led_counter = 0;
            GPIO_toggleDio(LED_PIN);
        }
    }
}

/* ─── Main ─── */

int main(void) {
    Task_Params taskParams;

    /* Board init */
    board_power_init();
    Power_init();

    VIMSConfigure(VIMS_BASE, TRUE, TRUE);
    VIMSModeSet(VIMS_BASE, VIMS_MODE_ENABLED);

#if !defined(POWER_SAVING)
    Power_setConstraint(PowerCC26XX_SB_DISALLOW);
    Power_setConstraint(PowerCC26XX_IDLE_PD_DISALLOW);
#endif

    board_gpio_init();

    /* Create RF semaphore */
    Semaphore_Params semParams;
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_COUNTING;
    g_rf_semaphore = Semaphore_construct(&s_rf_sem_struct, 0, &semParams);

    /* TX-done semaphore (unused for now but kept for forward compat) */
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    g_tx_done_semaphore = Semaphore_construct(&s_tx_done_sem_struct, 0, &semParams);

    /* BLE5-Stack ICall — disabled until Phase M3 */
#if 0
    user0Cfg.appServiceInfo->timerTickPeriod = Clock_tickPeriod;
    user0Cfg.appServiceInfo->timerMaxMillisecond = ICall_getMaxMSecs();
    ICall_init();
    ICall_createRemoteTasks();
#endif

    /* Create single main task (Sniffle uses priority 3) */
    Task_Params_init(&taskParams);
    taskParams.stack = s_main_task_stack;
    taskParams.stackSize = MAIN_TASK_STACK_SIZE;
    taskParams.priority = MAIN_TASK_PRIORITY;
    Task_construct(&s_main_task, MainTask_taskFxn, &taskParams, NULL);

    /* Start TI-RTOS kernel — never returns */
    BIOS_start();

    return 0;
}
