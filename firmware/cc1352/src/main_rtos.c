/*
 * FeralRF CC1352 — TI-RTOS Main Entry Point
 *
 * Fase 0.0: Skeleton — LED blink + UART COBS
 * Single task, no radio. Validates TI-RTOS kernel + UART path.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* TI-RTOS */
#include <ti/sysbios/BIOS.h>
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
#include DeviceFamily_constructPath(driverlib/vims.h)
/* clang-format on */

/* FeralRF app */
#include "command_processor.h"
#include "config.h"
#include "host_if.h"
#include "host_if_task.h"
#include "task_event.h"

/* ─── Task Configuration ─── */

#define MAIN_TASK_PRIORITY   3 /* Same as Sniffle — cooperative with RF SWIs */
#define MAIN_TASK_STACK_SIZE 4096

static Task_Struct s_main_task;
static uint8_t s_main_task_stack[MAIN_TASK_STACK_SIZE];

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

/* ─── Main Task ─── */

static void MainTask_taskFxn(UArg a0, UArg a1) {
    (void)a0;
    (void)a1;

    /* Initialize subsystems */
    HostIF_init();
    TaskEvent_init();
    CommandProcessor_init();
    HostIFTask_init();

    /* Main loop: UART polling + LED blink */
    uint32_t led_counter = 0;
    while (1) {
        /* Poll UART — read bytes, parse commands, send responses */
        HostIFTask_poll();

        /* LED blink */
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
