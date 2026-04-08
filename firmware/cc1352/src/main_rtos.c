/*
 * FeralRF CC1352 — TI-RTOS Main Entry Point
 *
 * Two-task architecture (like Sniffle):
 * - UART Task: command processing + TX output (high priority)
 * - RF Task: radio operations + packet processing (medium priority)
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* TI-RTOS */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Task.h>

/* TI Drivers */
#include <ti/drivers/GPIO.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>
#include <ti/drivers/rf/RF.h>

/* Device */
#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/gpio.h)
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/prcm.h)
#include DeviceFamily_constructPath(driverlib/sys_ctrl.h)
#include DeviceFamily_constructPath(driverlib/vims.h)
/* clang-format on */

/* BLE5-Stack — disabled until Phase M3 (GATT) */
#if 0
#define ICALL_JT
#define ICALL_LITE
#include "ble_user_config.h"
#endif

/* FeralRF app */
#include "command_processor.h"
#include "config.h"
#include "control_task.h"
#include "data_task.h"
#include "host_if.h"
#include "host_if_task.h"
#include "radio_if.h"
#include "smartrf_prop_0.h"
#include "task_event.h"
#include "ti_radio_config_433.h"

/* ─── Task Configuration ─── */

#define UART_TASK_PRIORITY 3 /* Same priority — cooperative via Task_yield */
#define UART_TASK_STACK_SIZE 4096

#define RF_TASK_PRIORITY 3 /* Same priority — cooperative via Task_yield */
#define RF_TASK_STACK_SIZE 4096

static Task_Struct s_uart_task;
static uint8_t s_uart_task_stack[UART_TASK_STACK_SIZE];

static Task_Struct s_rf_task;
static uint8_t s_rf_task_stack[RF_TASK_STACK_SIZE];

/* Semaphore for RF callback → RF task notification */
Semaphore_Struct s_rf_sem_struct;
Semaphore_Handle g_rf_semaphore = NULL;

/* Semaphore for UART → RF task command deferral */
static Semaphore_Struct s_rf_cmd_sem;

void RfTask_signalCommand(void) {
    Semaphore_post(Semaphore_handle(&s_rf_cmd_sem));
}

/* ─── Board Init ─── */

static void board_power_init(void) {
    PRCMPowerDomainOn(PRCM_DOMAIN_PERIPH | PRCM_DOMAIN_SERIAL);
    while (PRCMPowerDomainsAllOn(PRCM_DOMAIN_PERIPH | PRCM_DOMAIN_SERIAL) != PRCM_DOMAIN_POWER_ON) {
    }
    PRCMPeripheralRunEnable(PRCM_PERIPH_GPIO);
    PRCMPeripheralRunEnable(PRCM_PERIPH_UART0);
    PRCMLoadSet();
    while (!PRCMLoadGet()) {
    }
}

static void board_gpio_init(void) {
    IOCPortConfigureSet(LED_PIN, IOC_PORT_GPIO,
                        IOC_CURRENT_8MA | IOC_STRENGTH_MAX | IOC_NO_IOPULL | IOC_SLEW_DISABLE |
                            IOC_HYST_DISABLE | IOC_NO_EDGE | IOC_INT_DISABLE | IOC_IOMODE_NORMAL |
                            IOC_NO_WAKE_UP | IOC_INPUT_DISABLE);
    GPIO_setOutputEnableDio(LED_PIN, GPIO_OUTPUT_ENABLE);
#if LED_ACTIVE_LOW
    GPIO_setDio(LED_PIN);
#else
    GPIO_clearDio(LED_PIN);
#endif

    /* Antenna switching handled dynamically by SysConfig callback
     * (rfDriverCallbackAntennaSwitching in ti_drivers_config.c).
     * Do NOT force DIO28/29/30 here — it overrides the RF driver's
     * dynamic switching and breaks 2.4 GHz (BLE/IEEE). */
}

/* ─── UART Task: commands + responses ─── */

static void UartTask_taskFxn(UArg a0, UArg a1) {
    (void)a0;
    (void)a1;

    /* Initialize UART + command subsystems */
    HostIF_init();
    TaskEvent_init();
    ControlTask_init();
    CommandProcessor_init();
    HostIFTask_init();

    /* UART polling loop — always responsive */
    uint32_t led_counter = 0;
    while (1) {
        HostIFTask_poll();

        /* LED blink */
        led_counter++;
        if (led_counter >= 50000u) {
            led_counter = 0;
            GPIO_toggleDio(LED_PIN);
        }

        Task_yield(); /* Cooperative — let RF task run */
    }
}

/* ─── RF Task: radio processing ─── */

static void RfTask_taskFxn(UArg a0, UArg a1) {
    (void)a0;
    (void)a1;

    /* Open 433 MHz RF handle ONCE at boot with dedicated SysConfig structs.
     * NEVER RF_close — TI RF driver doesn't reset RF_core.init on close,
     * so subsequent RF_open skips CPE patch loading and RAT sync.
     * The driver caches pRadioSetup + first CMD_FS pointers. */
    {
        RF_Params rfParams;
        RF_Params_init(&rfParams);

        /* Set FS to TX mode before first use — gets cached by driver */
        Prop0_cmdFs433.synthConf.bTxMode = 1u;

        RF_Handle h = RF_open(RadioIF_get433Object(), &Prop0_mode433,
                              (RF_RadioSetup *)&Prop0_cmdPropRadioDivSetup433, &rfParams);
        if (h != NULL) {
            /* Seed the driver's FS cache at 433 MHz */
            RF_postCmd(h, (RF_Op *)&Prop0_cmdFs433, RF_PriorityNormal, NULL, 0);
            RadioIF_set433Handle(h);
        }
    }

    DataTask_init();
    while (1) {
        DataTask_poll();
        Task_yield();
    }
}

/* ─── Main ─── */

int main(void) {
    Task_Params taskParams;

    /* Board init */
    board_power_init();
    Power_init();
    GPIO_init(); /* Required for antenna switching (SysConfig callback) */

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
    semParams.mode = Semaphore_Mode_BINARY;
    g_rf_semaphore = Semaphore_construct(&s_rf_sem_struct, 0, &semParams);

    /* Create command deferral semaphore */
    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    Semaphore_construct(&s_rf_cmd_sem, 0, &semParams);

    /* BLE5-Stack ICall — disabled until Phase M3 */
#if 0
    user0Cfg.appServiceInfo->timerTickPeriod = Clock_tickPeriod;
    user0Cfg.appServiceInfo->timerMaxMillisecond = ICall_getMaxMSecs();
    ICall_init();
    ICall_createRemoteTasks();
#endif

    /* Create RF task FIRST — must run boot RF_open before UART */
    Task_Params_init(&taskParams);
    taskParams.stack = s_rf_task_stack;
    taskParams.stackSize = RF_TASK_STACK_SIZE;
    taskParams.priority = RF_TASK_PRIORITY;
    Task_construct(&s_rf_task, RfTask_taskFxn, &taskParams, NULL);

    /* Create UART task second */
    Task_Params_init(&taskParams);
    taskParams.stack = s_uart_task_stack;
    taskParams.stackSize = UART_TASK_STACK_SIZE;
    taskParams.priority = UART_TASK_PRIORITY;
    Task_construct(&s_uart_task, UartTask_taskFxn, &taskParams, NULL);

    /* Start TI-RTOS kernel — never returns */
    BIOS_start();

    return 0;
}
