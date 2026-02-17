/*
 * Minimal TI driver configuration required by RF multiMode + NoRTOS DPL.
 */

#include <stdbool.h>
#include <stdint.h>

#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/gpio.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/ioc.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26X2.h>
#include <ti/drivers/rf/RF.h>
#include <ti/drivers/temperature/TemperatureCC26X2.h>

/* CatSniffer RF switch control pins (matching reference sniffer config). */
#define RF_SW_24GHZ_DIO 28u
#define RF_SW_HIGH_PA_DIO 29u
#define RF_SW_SUB1_DIO 30u

static bool s_rf_switch_initialized = false;

static void rfSwitchInitPins(void) {
    IOCPortConfigureSet(RF_SW_24GHZ_DIO, IOC_PORT_GPIO,
                        IOC_CURRENT_8MA | IOC_STRENGTH_MAX | IOC_NO_IOPULL | IOC_SLEW_DISABLE |
                            IOC_HYST_DISABLE | IOC_NO_EDGE | IOC_INT_DISABLE | IOC_IOMODE_NORMAL |
                            IOC_NO_WAKE_UP | IOC_INPUT_DISABLE);
    IOCPortConfigureSet(RF_SW_HIGH_PA_DIO, IOC_PORT_GPIO,
                        IOC_CURRENT_8MA | IOC_STRENGTH_MAX | IOC_NO_IOPULL | IOC_SLEW_DISABLE |
                            IOC_HYST_DISABLE | IOC_NO_EDGE | IOC_INT_DISABLE | IOC_IOMODE_NORMAL |
                            IOC_NO_WAKE_UP | IOC_INPUT_DISABLE);
    IOCPortConfigureSet(RF_SW_SUB1_DIO, IOC_PORT_GPIO,
                        IOC_CURRENT_8MA | IOC_STRENGTH_MAX | IOC_NO_IOPULL | IOC_SLEW_DISABLE |
                            IOC_HYST_DISABLE | IOC_NO_EDGE | IOC_INT_DISABLE | IOC_IOMODE_NORMAL |
                            IOC_NO_WAKE_UP | IOC_INPUT_DISABLE);

    GPIO_setOutputEnableDio(RF_SW_24GHZ_DIO, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(RF_SW_HIGH_PA_DIO, GPIO_OUTPUT_ENABLE);
    GPIO_setOutputEnableDio(RF_SW_SUB1_DIO, GPIO_OUTPUT_ENABLE);

    GPIO_clearDio(RF_SW_24GHZ_DIO);
    GPIO_clearDio(RF_SW_HIGH_PA_DIO);
    GPIO_clearDio(RF_SW_SUB1_DIO);

    s_rf_switch_initialized = true;
}

static void rfSwitchSelect24GHz(void) {
    if (!s_rf_switch_initialized) {
        rfSwitchInitPins();
    }

    GPIO_setDio(RF_SW_24GHZ_DIO);
    GPIO_clearDio(RF_SW_HIGH_PA_DIO);
    GPIO_clearDio(RF_SW_SUB1_DIO);
}

static void rfSwitchAllOff(void) {
    if (!s_rf_switch_initialized) {
        rfSwitchInitPins();
    }

    GPIO_clearDio(RF_SW_24GHZ_DIO);
    GPIO_clearDio(RF_SW_HIGH_PA_DIO);
    GPIO_clearDio(RF_SW_SUB1_DIO);
}

void __attribute__((weak)) rfDriverCallback(RF_Handle client, RF_GlobalEvent events, void *arg) {
    (void)client;
    (void)events;
    (void)arg;
}

void __attribute__((weak))
rfDriverCallbackAntennaSwitching(RF_Handle client, RF_GlobalEvent events, void *arg) {
    (void)client;
    (void)arg;

    if ((events & RF_GlobalEventInit) != 0u) {
        if (!s_rf_switch_initialized) {
            rfSwitchInitPins();
        }
    }

    if ((events & RF_GlobalEventRadioSetup) != 0u) {
        /* Current backend is BLE 2.4 GHz RX, route RF front-end to 2.4 path. */
        rfSwitchSelect24GHz();
    }

    if ((events & RF_GlobalEventRadioPowerDown) != 0u) {
        rfSwitchAllOff();
    }
}

static void RF_globalCallbackFunction(RF_Handle client, RF_GlobalEvent events, void *arg) {
    rfDriverCallback(client, events, arg);
    rfDriverCallbackAntennaSwitching(client, events, arg);
}

const PowerCC26X2_Config PowerCC26X2_config = {
    .policyInitFxn = NULL,
    .policyFxn = &PowerCC26XX_standbyPolicy,
    .calibrateFxn = &PowerCC26XX_calibrate,
    .enablePolicy = true,
    .calibrateRCOSC_LF = true,
    .calibrateRCOSC_HF = true,
    .enableTCXOFxn = NULL,
};

const TemperatureCC26X2_Config TemperatureCC26X2_config = {
    .intPriority = (uint8_t)~0,
};

const RFCC26XX_HWAttrsV2 RFCC26XX_hwAttrs = {
    .hwiPriority = (uint8_t)~0,
    .swiPriority = 0,
    .xoscHfAlwaysNeeded = true,
    .globalCallback = &RF_globalCallbackFunction,
    .globalEventMask = RF_GlobalEventInit | RF_GlobalEventRadioPowerDown | RF_GlobalEventRadioSetup,
};
