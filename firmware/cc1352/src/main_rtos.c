/*
 * FeralRF CC1352 — TI-RTOS Main Entry Point
 *
 * Fase 0.1 test: single task with BLE RX following rfDiagnostics pattern.
 * RF_open → RF_postCmd(FS) → RF_postCmd(GENERIC_RX) → callback count via stats.
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
#include <ti/drivers/rf/RF.h>

/* Device */
#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/gpio.h)
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include DeviceFamily_constructPath(driverlib/prcm.h)
#include DeviceFamily_constructPath(driverlib/vims.h)
#include DeviceFamily_constructPath(driverlib/rf_data_entry.h)
#include DeviceFamily_constructPath(driverlib/cpu.h)
/* clang-format on */

/* FeralRF app */
#include "command_processor.h"
#include "config.h"
#include "host_if.h"
#include "host_if_task.h"
#include "task_event.h"
#include "smartrf_ble5_0.h"

/* ─── Task Configuration ─── */

#define MAIN_TASK_PRIORITY   3
#define MAIN_TASK_STACK_SIZE 4096

static Task_Struct s_main_task;
static uint8_t s_main_task_stack[MAIN_TASK_STACK_SIZE];

/* ─── BLE RX Test State ─── */

#define RX_NUM_ENTRIES   2
#define RX_MAX_LEN       270
#define RX_ENTRY_HDR     8
#define RX_ENTRY_PAD(l)  ((4u - (((l) + RX_ENTRY_HDR) % 4u)) % 4u)
#define RX_ENTRY_SIZE    (RX_ENTRY_HDR + RX_MAX_LEN + RX_ENTRY_PAD(RX_MAX_LEN))

static uint8_t s_rx_buf[RX_NUM_ENTRIES * RX_ENTRY_SIZE] __attribute__((aligned(4)));
static dataQueue_t s_rx_queue;

static RF_Object s_rf_object;
static RF_Handle s_rf_handle = NULL;
static RF_CmdHandle s_rx_cmd_handle = (RF_CmdHandle)-1;

/* Counters readable via GET_STATS (hacked into command_processor) */
volatile uint32_t g_ble_rx_callback_count = 0;
volatile uint32_t g_ble_rx_entry_done = 0;
volatile uint32_t g_ble_rx_cmd_status = 0;
volatile uint32_t g_rf_open_result = 0;

static void ble_rx_callback(RF_Handle h, RF_CmdHandle ch, RF_EventMask e) {
    (void)h;
    (void)ch;
    g_ble_rx_callback_count++;

    if (e & RF_EventRxEntryDone) {
        g_ble_rx_entry_done++;
        /* Consume entry to prevent queue overflow (Sniffle pattern) */
        rfc_dataEntryGeneral_t *entry =
            (rfc_dataEntryGeneral_t *)s_rx_queue.pCurrEntry;
        if (entry != NULL && entry->status == DATA_ENTRY_FINISHED) {
            entry->status = DATA_ENTRY_PENDING;
            s_rx_queue.pCurrEntry = entry->pNextEntry;
        }
    }
}

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

    /* Initialize UART subsystems */
    HostIF_init();
    TaskEvent_init();
    CommandProcessor_init();
    HostIFTask_init();

    /* === BLE RX Test: rfDiagnostics pattern === */

    /* 1. Set up circular data queue */
    uint8_t *first = s_rx_buf;
    for (uint8_t i = 0; i < RX_NUM_ENTRIES; i++) {
        rfc_dataEntryGeneral_t *e =
            (rfc_dataEntryGeneral_t *)(first + i * RX_ENTRY_SIZE);
        e->status = DATA_ENTRY_PENDING;
        e->config.type = DATA_ENTRY_TYPE_GEN;
        e->config.lenSz = 2;
        e->length = RX_MAX_LEN;
        e->pNextEntry = first + ((i + 1) % RX_NUM_ENTRIES) * RX_ENTRY_SIZE;
    }
    s_rx_queue.pCurrEntry = first;
    s_rx_queue.pLastEntry = NULL;

    /* 2. Configure GENERIC_RX command */
    Ble5_0_cmdBle5GenericRx.pParams->pRxQ = &s_rx_queue;
    Ble5_0_cmdBle5GenericRx.channel = 0x25; /* ch37 = 2402 MHz */
    Ble5_0_cmdBle5GenericRx.whitening.init = 0x65;
    Ble5_0_cmdBle5GenericRx.whitening.bOverride = 1;

    /* 3. RF_open (runs RadioSetup internally) */
    RF_Params rfParams;
    RF_Params_init(&rfParams);
    s_rf_handle = RF_open(&s_rf_object, &Ble5_0_mode,
                           (RF_RadioSetup *)&Ble5_0_cmdBle5RadioSetup, &rfParams);

    g_rf_open_result = (s_rf_handle != NULL) ? 0xC0DE : 0xDEAD;

    if (s_rf_handle != NULL) {
        /* Direct RF_postCmd(GENERIC_RX) — no CMD_FS for BLE */
        Ble5_0_cmdBle5GenericRx.status = 0x0000;
        s_rx_cmd_handle = RF_postCmd(s_rf_handle,
                                      (RF_Op *)&Ble5_0_cmdBle5GenericRx,
                                      RF_PriorityNormal,
                                      &ble_rx_callback,
                                      RF_EventRxEntryDone);
        /* Store postCmd result as cmd_status debug field */
        g_ble_rx_cmd_status = (uint32_t)(int32_t)s_rx_cmd_handle;
    }

    /* Main loop: UART polling + LED blink */
    uint32_t led_counter = 0;
    while (1) {
        HostIFTask_poll();

        /* DEBUG: RX cmd status (high 16) | postCmd handle (low 16) */
        g_ble_rx_cmd_status = ((uint32_t)Ble5_0_cmdBle5GenericRx.status << 16) |
                               ((uint32_t)(int32_t)s_rx_cmd_handle & 0xFFFFu);

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

    board_power_init();
    Power_init();

    VIMSConfigure(VIMS_BASE, TRUE, TRUE);
    VIMSModeSet(VIMS_BASE, VIMS_MODE_ENABLED);

#if !defined(POWER_SAVING)
    Power_setConstraint(PowerCC26XX_SB_DISALLOW);
    Power_setConstraint(PowerCC26XX_IDLE_PD_DISALLOW);
#endif

    board_gpio_init();

    Task_Params_init(&taskParams);
    taskParams.stack = s_main_task_stack;
    taskParams.stackSize = MAIN_TASK_STACK_SIZE;
    taskParams.priority = MAIN_TASK_PRIORITY;
    Task_construct(&s_main_task, MainTask_taskFxn, &taskParams, NULL);

    BIOS_start();

    return 0;
}
