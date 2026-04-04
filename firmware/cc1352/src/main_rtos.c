/*
 * FeralRF CC1352 — TI-RTOS Main Entry Point
 *
 * Phase M2: TI-RTOS + BLE5-Stack (ICall + GATT client)
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* TI-RTOS */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/knl/Event.h>

/* TI Drivers */
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>

/* BLE5-Stack */
#define ICALL_JT
#define ICALL_LITE
#define CC13XX
#define CC13X2P
#define SYSCFG
#define BLE_V50_FEATURES (PHY_2MBPS_CFG | PHY_LR_CFG)
#define PHY_2MBPS_CFG    0x01
#define PHY_LR_CFG       0x02
#include <icall.h>
#include "icall_user_config.h"
#include "ble_user_config.h"

/* Device */
#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/gpio.h)
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/prcm.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include DeviceFamily_constructPath(driverlib/vims.h)
/* clang-format on */

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

#define FERALRF_TASK_PRIORITY   1
#define FERALRF_TASK_STACK_SIZE 2048

static Task_Struct s_feralrf_task;
static uint8_t s_feralrf_task_stack[FERALRF_TASK_STACK_SIZE];

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

/* ─── FeralRF App Task ─── */

static void FeralRF_taskFxn(UArg a0, UArg a1) {
    (void)a0;
    (void)a1;

    /* Initialize subsystems */
    HostIF_init();
    TaskEvent_init();
    ControlTask_init();
    CommandProcessor_init();
    DataTask_init();
    HostIFTask_init();

    /* Main loop — polling inside RTOS task */
    uint32_t led_counter = 0;
    while (1) {
        HostIFTask_poll();
        DataTask_poll();

        /* LED blink via counter (non-blocking) */
        led_counter++;
        if (led_counter >= 50000u) {
            led_counter = 0;
            GPIO_toggleDio(LED_PIN);
        }

        Task_yield();
    }
}

static void FeralRF_createTask(void) {
    Task_Params taskParams;
    Task_Params_init(&taskParams);
    taskParams.stack = s_feralrf_task_stack;
    taskParams.stackSize = FERALRF_TASK_STACK_SIZE;
    taskParams.priority = FERALRF_TASK_PRIORITY;
    Task_construct(&s_feralrf_task, FeralRF_taskFxn, &taskParams, NULL);
}

/* ─── Main ─── */

int main(void) {
    /* Board init */
    board_power_init();
    Power_init();

    /* Enable instruction cache */
    VIMSConfigure(VIMS_BASE, TRUE, TRUE);
    VIMSModeSet(VIMS_BASE, VIMS_MODE_ENABLED);

#if !defined(POWER_SAVING)
    Power_setConstraint(PowerCC26XX_SB_DISALLOW);
    Power_setConstraint(PowerCC26XX_IDLE_PD_DISALLOW);
#endif

    /* GPIO init */
    board_gpio_init();

    /* BLE5-Stack timer config */
    user0Cfg.appServiceInfo->timerTickPeriod = Clock_tickPeriod;
    user0Cfg.appServiceInfo->timerMaxMillisecond = ICall_getMaxMSecs();

    /* Initialize ICall — BLE stack message framework */
    ICall_init();

    /* Start BLE stack tasks — Priority 5 */
    ICall_createRemoteTasks();

    /* Create FeralRF application task — Priority 1 */
    FeralRF_createTask();

    /* Start TI-RTOS kernel — never returns */
    BIOS_start();

    return 0;
}
