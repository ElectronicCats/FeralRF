/*
 *  ======== ti_drivers_config.c ========
 *  Configured TI-Drivers module definitions
 *
 *  DO NOT EDIT - This file is generated for the LP_CC1352P7_4
 *  by the SysConfig tool.
 */

#include <stddef.h>
#include <stdint.h>

#ifndef DeviceFamily_CC13X2X7
#define DeviceFamily_CC13X2X7
#endif

#include <ti/devices/DeviceFamily.h>

#include "ti_drivers_config.h"

/*
 *  =============================== GPIO ===============================
 */

#include <ti/drivers/GPIO.h>
#include <ti/drivers/gpio/GPIOCC26XX.h>

/* The range of pins available on this device */
const uint_least8_t GPIO_pinLowerBound = 5;
const uint_least8_t GPIO_pinUpperBound = 30;

/*
 *  ======== gpioPinConfigs ========
 *  Array of Pin configurations
 */
GPIO_PinConfig gpioPinConfigs[31] = {
    0,               /* Pin is not available on this device */
    0,               /* Pin is not available on this device */
    0,               /* Pin is not available on this device */
    0,               /* Pin is not available on this device */
    0,               /* Pin is not available on this device */
    GPIO_CFG_NO_DIR, /* DIO_5 */
    GPIO_CFG_NO_DIR, /* DIO_6 */
    GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_MED | GPIO_CFG_OUT_LOW, /* CONFIG_GPIO_GLED */
    GPIO_CFG_NO_DIR,                                                    /* DIO_8 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_9 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_10 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_11 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_12 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_13 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_14 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_15 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_16 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_17 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_18 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_19 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_20 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_21 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_22 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_23 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_24 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_25 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_26 */
    GPIO_CFG_NO_DIR,                                                    /* DIO_27 */
    /* Owned by /ti/drivers/RF as RF Antenna Pin 0 */
    GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_HIGH | GPIO_CFG_OUT_LOW, /* CONFIG_RF_24GHZ */
    /* Owned by /ti/drivers/RF as RF Antenna Pin 1 */
    GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_HIGH | GPIO_CFG_OUT_LOW, /* CONFIG_RF_HIGH_PA */
    /* Owned by /ti/drivers/RF as RF Antenna Pin 2 */
    GPIO_CFG_OUTPUT_INTERNAL | GPIO_CFG_OUT_STR_HIGH | GPIO_CFG_OUT_LOW, /* CONFIG_RF_SUB1GHZ */
};

/*
 *  ======== gpioCallbackFunctions ========
 *  Array of callback function pointers
 *  Change at runtime with GPIO_setCallback()
 */
GPIO_CallbackFxn gpioCallbackFunctions[31];

/*
 *  ======== gpioUserArgs ========
 *  Array of user argument pointers
 *  Change at runtime with GPIO_setUserArg()
 *  Get values with GPIO_getUserArg()
 */
void *gpioUserArgs[31];

const uint_least8_t CONFIG_RF_24GHZ_CONST = CONFIG_RF_24GHZ;
const uint_least8_t CONFIG_RF_HIGH_PA_CONST = CONFIG_RF_HIGH_PA;
const uint_least8_t CONFIG_RF_SUB1GHZ_CONST = CONFIG_RF_SUB1GHZ;
const uint_least8_t CONFIG_GPIO_GLED_CONST = CONFIG_GPIO_GLED;

/*
 *  ======== GPIO_config ========
 */
const GPIO_Config GPIO_config = {.configs = (GPIO_PinConfig *)gpioPinConfigs,
                                 .callbacks = (GPIO_CallbackFxn *)gpioCallbackFunctions,
                                 .userArgs = gpioUserArgs,
                                 .intPriority = (~0)};

/*
 *  =============================== Power ===============================
 */
#include "ti_drivers_config.h"
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26X2.h>

extern void PowerCC26XX_standbyPolicy(void);
extern bool PowerCC26XX_calibrate(unsigned int);

const PowerCC26X2_Config PowerCC26X2_config = {.enablePolicy = true,
                                               .policyInitFxn = NULL,
                                               .policyFxn = PowerCC26XX_standbyPolicy,
                                               .calibrateFxn = PowerCC26XX_calibrate,
                                               .calibrateRCOSC_LF = true,
                                               .calibrateRCOSC_HF = true,
                                               .enableTCXOFxn = NULL};

/*
 *  =============================== RF Driver ===============================
 */
#include <ti/devices/DeviceFamily.h>
#include <ti/drivers/GPIO.h>
#include DeviceFamily_constructPath(driverlib/ioc.h)
#include <ti/drivers/rf/RF.h>

/*
 * RF driver callback function, called by the driver on global driver events.
 */
static void RF_globalCallbackFunction(RF_Handle client, RF_GlobalEvent events, void *arg);

/*
 * Callback function to handle custom / application specific behavior
 */
extern void __attribute__((weak))
rfDriverCallback(RF_Handle client, RF_GlobalEvent events, void *arg);

/*
 * Callback function to handle antenna switching
 */
extern void __attribute__((weak))
rfDriverCallbackAntennaSwitching(RF_Handle client, RF_GlobalEvent events, void *arg);

/*
 * Platform-specific driver configuration
 */
const RFCC26XX_HWAttrsV2 RFCC26XX_hwAttrs = {.hwiPriority = (~0),
                                             .swiPriority = (uint8_t)0,
                                             .xoscHfAlwaysNeeded = true,
                                             .globalCallback = &RF_globalCallbackFunction,
                                             .globalEventMask = RF_GlobalEventInit |
                                                                RF_GlobalEventRadioPowerDown |
                                                                RF_GlobalEventRadioSetup};

/*
 *  ======== RF_globalCallbackFunction ========
 *  This function is called by the driver on global driver events.
 *  It will call specific callback functions to further handle the triggering events.
 */
static void RF_globalCallbackFunction(RF_Handle client, RF_GlobalEvent events, void *arg) {
    rfDriverCallback(client, events, arg);
    rfDriverCallbackAntennaSwitching(client, events, arg);
}

/*
 *  ======== rfDriverCallback ========
 *  Handle events triggered by the RF driver for custom / application specific behavior.
 */
void __attribute__((weak)) rfDriverCallback(RF_Handle client, RF_GlobalEvent events, void *arg) {
    /* ======== PLEASE READ THIS ========
     *
     * This function is declared weak for the application to override it.
     * A new definition of 'rfDriverCallback' is required if you want to
     * handle the events listed in '.globalEventMask'.
     *
     * Please copy this function definition to create your own, but make
     * sure to remove '__attribute__((weak))' for your definition.
     *
     * According to '.globalEventMask', this function will be triggered by:
     *   - RF_GlobalEventInit
     *   - RF_GlobalEventRadioPowerDown
     *   - RF_GlobalEventRadioSetup
     *
     * An example of how to handle these events would be:
     *
     *   --- Code snippet begin ---
     *
     *   if(events & RF_GlobalEventInit) {
     *       // Perform action for this event
     *   }
     *   else if (events & RF_GlobalEventRadioPowerDown) {
     *       // Perform action for this event
     *   }
     *   else if (events & RF_GlobalEventRadioSetup) {
     *       // Perform action for this event
     *   }
     *
     *   --- Code snippet end ---
     */
}

/*
 * ======== Antenna switching ========
 */
/*
 * ======== rfDriverCallbackAntennaSwitching ========
 * Sets up the antenna switch depending on the current PHY configuration.
 *
 * Truth table:
 *
 * Path       DIO28 DIO29 DIO30
 * ========== ===== ===== =====
 * Off        0     0     0
 * 2.4 GHZ    1     0     0
 * HIGH PA    0     1     0
 * SUB1 GHZ   0     0     1
 */
void __attribute__((weak))
rfDriverCallbackAntennaSwitching(RF_Handle client, RF_GlobalEvent events, void *arg) {

    if (events & RF_GlobalEventRadioSetup) {
        bool sub1GHz = false;
        uint8_t loDivider = 0;

        /* Switch off all paths. */
        GPIO_write(CONFIG_RF_24GHZ, 0);
        GPIO_write(CONFIG_RF_HIGH_PA, 0);
        GPIO_write(CONFIG_RF_SUB1GHZ, 0);

        /* Decode the current PA configuration. */
        RF_TxPowerTable_PAType paType = (RF_TxPowerTable_PAType)RF_getTxPower(client).paType;

        /* Decode the generic argument as a setup command. */
        RF_RadioSetup *setupCommand = (RF_RadioSetup *)arg;

        switch (setupCommand->common.commandNo) {
        case (CMD_RADIO_SETUP):
        case (CMD_BLE5_RADIO_SETUP):
            loDivider = RF_LODIVIDER_MASK & setupCommand->common.loDivider;

            /* Sub-1GHz front-end. */
            if (loDivider != 0) {
                sub1GHz = true;
            }
            break;
        case (CMD_PROP_RADIO_DIV_SETUP):
            loDivider = RF_LODIVIDER_MASK & setupCommand->prop_div.loDivider;

            /* Sub-1GHz front-end. */
            if (loDivider != 0) {
                sub1GHz = true;
            }
            break;
        default:
            break;
        }

        if (sub1GHz) {
            /* Sub-1 GHz */
            if (paType == RF_TxPowerTable_HighPA) {
                /* PA enable --> HIGH PA
                 * LNA enable --> Sub-1 GHz
                 */
                GPIO_setConfigAndMux(CONFIG_RF_24GHZ, GPIO_CFG_OUTPUT, IOC_PORT_GPIO);
                /* Note: RFC_GPO3 is a work-around because the RFC_GPO1 (PA enable signal) is
                   sometimes not de-asserted on CC1352 Rev A. */
                GPIO_setConfigAndMux(CONFIG_RF_HIGH_PA, GPIO_CFG_OUTPUT, IOC_PORT_RFC_GPO3);
                GPIO_setConfigAndMux(CONFIG_RF_SUB1GHZ, GPIO_CFG_OUTPUT, IOC_PORT_RFC_GPO0);
            } else {
                /* RF core active --> Sub-1 GHz */
                GPIO_setConfigAndMux(CONFIG_RF_24GHZ, GPIO_CFG_OUTPUT, IOC_PORT_GPIO);
                GPIO_setConfigAndMux(CONFIG_RF_HIGH_PA, GPIO_CFG_OUTPUT, IOC_PORT_GPIO);
                GPIO_setConfigAndMux(CONFIG_RF_SUB1GHZ, GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
                                     IOC_PORT_GPIO);
            }
        } else {
            /* 2.4 GHz */
            if (paType == RF_TxPowerTable_HighPA) {
                /* PA enable --> HIGH PA
                 * LNA enable --> 2.4 GHz
                 */
                GPIO_setConfigAndMux(CONFIG_RF_24GHZ, GPIO_CFG_OUTPUT, IOC_PORT_RFC_GPO0);
                /* Note: RFC_GPO3 is a work-around because the RFC_GPO1 (PA enable signal) is
                   sometimes not de-asserted on CC1352 Rev A. */
                GPIO_setConfigAndMux(CONFIG_RF_HIGH_PA, GPIO_CFG_OUTPUT, IOC_PORT_RFC_GPO3);
                GPIO_setConfigAndMux(CONFIG_RF_SUB1GHZ, GPIO_CFG_OUTPUT, IOC_PORT_GPIO);
            } else {
                /* RF core active --> 2.4 GHz */
                GPIO_setConfigAndMux(CONFIG_RF_24GHZ, GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH,
                                     IOC_PORT_GPIO);
                GPIO_setConfigAndMux(CONFIG_RF_HIGH_PA, GPIO_CFG_OUTPUT, IOC_PORT_GPIO);
                GPIO_setConfigAndMux(CONFIG_RF_SUB1GHZ, GPIO_CFG_OUTPUT, IOC_PORT_GPIO);
            }
        }
    } else if (events & RF_GlobalEventRadioPowerDown) {
        /* Switch off all paths. */
        GPIO_write(CONFIG_RF_24GHZ, 0);
        GPIO_write(CONFIG_RF_HIGH_PA, 0);
        GPIO_write(CONFIG_RF_SUB1GHZ, 0);

        /* Reset the IO multiplexer to GPIO functionality */
        GPIO_setConfigAndMux(CONFIG_RF_24GHZ, GPIO_CFG_OUTPUT, IOC_PORT_GPIO);
        GPIO_setConfigAndMux(CONFIG_RF_HIGH_PA, GPIO_CFG_OUTPUT, IOC_PORT_GPIO);
        GPIO_setConfigAndMux(CONFIG_RF_SUB1GHZ, GPIO_CFG_OUTPUT, IOC_PORT_GPIO);
    }
}

#include <stdbool.h>

#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/cpu.h>
#include <ti/devices/cc13x2x7_cc26x2x7/driverlib/ioc.h>

#include <ti/drivers/GPIO.h>

/* Board GPIO defines */
#define BOARD_EXT_FLASH_SPI_CS 20
#define BOARD_EXT_FLASH_SPI_CLK 10
#define BOARD_EXT_FLASH_SPI_PICO 9
#define BOARD_EXT_FLASH_SPI_POCI 8

/*
 *  ======== Board_sendExtFlashByte ========
 */
void Board_sendExtFlashByte(uint8_t byte) {
    uint8_t i;

    /* SPI Flash CS */
    GPIO_write(BOARD_EXT_FLASH_SPI_CS, 0);

    for (i = 0; i < 8; i++) {
        GPIO_write(BOARD_EXT_FLASH_SPI_CLK, 0); /* SPI Flash CLK */

        /* SPI Flash PICO */
        GPIO_write(BOARD_EXT_FLASH_SPI_PICO, (byte >> (7 - i)) & 0x01);
        GPIO_write(BOARD_EXT_FLASH_SPI_CLK, 1); /* SPI Flash CLK */

        /*
         * Waste a few cycles to keep the CLK high for at
         * least 45% of the period.
         * 3 cycles per loop: 8 loops @ 48 Mhz = 0.5 us.
         */
        CPUdelay(8);
    }

    GPIO_write(BOARD_EXT_FLASH_SPI_CLK, 0); /* CLK */
    GPIO_write(BOARD_EXT_FLASH_SPI_CS, 1);  /* CS */

    /*
     * Keep CS high at least 40 us
     * 3 cycles per loop: 700 loops @ 48 Mhz ~= 44 us
     */
    CPUdelay(700);
}

/*
 *  ======== Board_wakeUpExtFlash ========
 */
void Board_wakeUpExtFlash(void) {
    /* SPI Flash CS*/
    GPIO_setConfig(BOARD_EXT_FLASH_SPI_CS,
                   GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH | GPIO_CFG_OUT_STR_MED);

    /*
     *  To wake up we need to toggle the chip select at
     *  least 20 ns and ten wait at least 35 us.
     */

    /* Toggle chip select for ~20ns to wake ext. flash */
    GPIO_write(BOARD_EXT_FLASH_SPI_CS, 0);
    /* 3 cycles per loop: 1 loop @ 48 Mhz ~= 62 ns */
    CPUdelay(1);
    GPIO_write(BOARD_EXT_FLASH_SPI_CS, 1);
    /* 3 cycles per loop: 560 loops @ 48 Mhz ~= 35 us */
    CPUdelay(560);
}

/*
 *  ======== Board_shutDownExtFlash ========
 */
void Board_shutDownExtFlash(void) {
    /*
     *  To be sure we are putting the flash into sleep and not waking it,
     *  we first have to make a wake up call
     */
    Board_wakeUpExtFlash();

    /* SPI Flash CS*/
    GPIO_setConfig(BOARD_EXT_FLASH_SPI_CS,
                   GPIO_CFG_OUTPUT | GPIO_CFG_OUT_HIGH | GPIO_CFG_OUT_STR_MED);
    /* SPI Flash CLK */
    GPIO_setConfig(BOARD_EXT_FLASH_SPI_CLK,
                   GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW | GPIO_CFG_OUT_STR_MED);
    /* SPI Flash PICO */
    GPIO_setConfig(BOARD_EXT_FLASH_SPI_PICO,
                   GPIO_CFG_OUTPUT | GPIO_CFG_OUT_LOW | GPIO_CFG_OUT_STR_MED);
    /* SPI Flash POCI */
    GPIO_setConfig(BOARD_EXT_FLASH_SPI_POCI, GPIO_CFG_IN_PD);

    uint8_t extFlashShutdown = 0xB9;

    Board_sendExtFlashByte(extFlashShutdown);

    GPIO_resetConfig(BOARD_EXT_FLASH_SPI_CS);
    GPIO_resetConfig(BOARD_EXT_FLASH_SPI_CLK);
    GPIO_resetConfig(BOARD_EXT_FLASH_SPI_PICO);
    GPIO_resetConfig(BOARD_EXT_FLASH_SPI_POCI);
}

/*
 *  =============================== TRNG ===============================
 */

#include <ti/drivers/TRNG.h>
#include <ti/drivers/trng/TRNGCC26XX.h>

TRNGCC26XX_Object trngCC26XXObjects[CONFIG_TRNG_COUNT];

/*
 *  ======== trngCC26XXHWAttrs ========
 */
const TRNGCC26XX_HWAttrs trngCC26XXHWAttrs[CONFIG_TRNG_COUNT] = {
    {
        .intPriority = (~0),
        .swiPriority = 0,
        .samplesPerCycle = 240000,
    },
};

const TRNG_Config TRNG_config[CONFIG_TRNG_COUNT] = {
    {   /* CONFIG_TRNG_0 */
        .object  = &trngCC26XXObjects[CONFIG_TRNG_0],
        .hwAttrs = &trngCC26XXHWAttrs[CONFIG_TRNG_0]
    },
};

const uint_least8_t CONFIG_TRNG_0_CONST = CONFIG_TRNG_0;
const uint_least8_t TRNG_count = CONFIG_TRNG_COUNT;

/*
 *  =============================== AESECB ===============================
 */

#include <ti/drivers/AESECB.h>
#include <ti/drivers/aesecb/AESECBCC26XX.h>

AESECBCC26XX_Object aesecbCC26XXObjects[CONFIG_AESECB_COUNT];

/*
 *  ======== aesecbCC26XXHWAttrs ========
 */
const AESECBCC26XX_HWAttrs aesecbCC26XXHWAttrs[CONFIG_AESECB_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const AESECB_Config AESECB_config[CONFIG_AESECB_COUNT] = {
    {   /* CONFIG_AESECB_0 */
        .object  = &aesecbCC26XXObjects[CONFIG_AESECB_0],
        .hwAttrs = &aesecbCC26XXHWAttrs[CONFIG_AESECB_0]
    },
};

const uint_least8_t CONFIG_AESECB_0_CONST = CONFIG_AESECB_0;
const uint_least8_t AESECB_count = CONFIG_AESECB_COUNT;

/*
 *  =============================== AESCCM ===============================
 */

#include <ti/drivers/AESCCM.h>
#include <ti/drivers/aesccm/AESCCMCC26XX.h>

AESCCMCC26XX_Object aesccmCC26XXObjects[CONFIG_AESCCM_COUNT];

/*
 *  ======== aesccmCC26XXHWAttrs ========
 */
const AESCCMCC26XX_HWAttrs aesccmCC26XXHWAttrs[CONFIG_AESCCM_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const AESCCM_Config AESCCM_config[CONFIG_AESCCM_COUNT] = {
    {   /* CONFIG_AESCCM_0 */
        .object  = &aesccmCC26XXObjects[CONFIG_AESCCM_0],
        .hwAttrs = &aesccmCC26XXHWAttrs[CONFIG_AESCCM_0]
    },
};

const uint_least8_t CONFIG_AESCCM_0_CONST = CONFIG_AESCCM_0;
const uint_least8_t AESCCM_count = CONFIG_AESCCM_COUNT;

/*
 *  =============================== AESCTR ===============================
 */

#include <ti/drivers/AESCTR.h>
#include <ti/drivers/aesctr/AESCTRCC26XX.h>

AESCTRCC26XX_Object aesctrCC26XXObjects[CONFIG_AESCTR_COUNT];

/*
 *  ======== aesctrCC26XXHWAttrs ========
 */
const AESCTRCC26XX_HWAttrs aesctrCC26XXHWAttrs[CONFIG_AESCTR_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const AESCTR_Config AESCTR_config[CONFIG_AESCTR_COUNT] = {
    {   /* CONFIG_AESCTR_0 */
        .object  = &aesctrCC26XXObjects[CONFIG_AESCTR_0],
        .hwAttrs = &aesctrCC26XXHWAttrs[CONFIG_AESCTR_0]
    },
};

const uint_least8_t CONFIG_AESCTR_0_CONST = CONFIG_AESCTR_0;
const uint_least8_t AESCTR_count = CONFIG_AESCTR_COUNT;

/*
 *  =============================== AESCBC ===============================
 */

#include <ti/drivers/AESCBC.h>
#include <ti/drivers/aescbc/AESCBCCC26XX.h>

AESCBCCC26XX_Object aescbcCC26XXObjects[CONFIG_AESCBC_COUNT];

/*
 *  ======== aescbcCC26XXHWAttrs ========
 */
const AESCBCCC26XX_HWAttrs aescbcCC26XXHWAttrs[CONFIG_AESCBC_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const AESCBC_Config AESCBC_config[CONFIG_AESCBC_COUNT] = {
    {   /* CONFIG_AESCBC_0 */
        .object  = &aescbcCC26XXObjects[CONFIG_AESCBC_0],
        .hwAttrs = &aescbcCC26XXHWAttrs[CONFIG_AESCBC_0]
    },
};

const uint_least8_t CONFIG_AESCBC_0_CONST = CONFIG_AESCBC_0;
const uint_least8_t AESCBC_count = CONFIG_AESCBC_COUNT;

/*
 *  =============================== AESGCM ===============================
 */

#include <ti/drivers/AESGCM.h>
#include <ti/drivers/aesgcm/AESGCMCC26XX.h>

AESGCMCC26XX_Object aesgcmCC26XXObjects[CONFIG_AESGCM_COUNT];

/*
 *  ======== aesgcmCC26XXHWAttrs ========
 */
const AESGCMCC26XX_HWAttrs aesgcmCC26XXHWAttrs[CONFIG_AESGCM_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const AESGCM_Config AESGCM_config[CONFIG_AESGCM_COUNT] = {
    {   /* CONFIG_AESGCM_0 */
        .object  = &aesgcmCC26XXObjects[CONFIG_AESGCM_0],
        .hwAttrs = &aesgcmCC26XXHWAttrs[CONFIG_AESGCM_0]
    },
};

const uint_least8_t CONFIG_AESGCM_0_CONST = CONFIG_AESGCM_0;
const uint_least8_t AESGCM_count = CONFIG_AESGCM_COUNT;

/*
 *  =============================== SHA2 ===============================
 */

#include <ti/drivers/SHA2.h>
#include <ti/drivers/sha2/SHA2CC26X2.h>

SHA2CC26X2_Object sha2CC26X2Objects[CONFIG_SHA2_COUNT];

/*
 *  ======== sha2CC26X2HWAttrs ========
 */
const SHA2CC26X2_HWAttrs sha2CC26X2HWAttrs[CONFIG_SHA2_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const SHA2_Config SHA2_config[CONFIG_SHA2_COUNT] = {
    {   /* CONFIG_SHA2_0 */
        .object  = &sha2CC26X2Objects[CONFIG_SHA2_0],
        .hwAttrs = &sha2CC26X2HWAttrs[CONFIG_SHA2_0]
    },
};

const uint_least8_t CONFIG_SHA2_0_CONST = CONFIG_SHA2_0;
const uint_least8_t SHA2_count = CONFIG_SHA2_COUNT;

/*
 *  =============================== ECDH ===============================
 */

#include <ti/drivers/ECDH.h>
#include <ti/drivers/ecdh/ECDHCC26X2.h>

ECDHCC26X2_Object ecdhCC26X2Objects[CONFIG_ECDH_COUNT];

/*
 *  ======== ecdhCC26X2HWAttrs ========
 */
const ECDHCC26X2_HWAttrs ecdhCC26X2HWAttrs[CONFIG_ECDH_COUNT] = {
    {
        .intPriority = (~0),
    },
};

const ECDH_Config ECDH_config[CONFIG_ECDH_COUNT] = {
    {   /* CONFIG_ECDH_0 */
        .object  = &ecdhCC26X2Objects[CONFIG_ECDH_0],
        .hwAttrs = &ecdhCC26X2HWAttrs[CONFIG_ECDH_0]
    },
};

const uint_least8_t CONFIG_ECDH_0_CONST = CONFIG_ECDH_0;
const uint_least8_t ECDH_count = CONFIG_ECDH_COUNT;

/*
 *  =============================== ECDSA ===============================
 */

#include <ti/drivers/ECDSA.h>
#include <ti/drivers/ecdsa/ECDSACC26X2.h>

ECDSACC26X2_Object ecdsaCC26X2Objects[CONFIG_ECDSA_COUNT];

/*
 *  ======== ecdsaCC26X2HWAttrs ========
 */
const ECDSACC26X2_HWAttrs ecdsaCC26X2HWAttrs[CONFIG_ECDSA_COUNT] = {
    {
        .intPriority = (~0),
        .trngIntPriority = (~0),
    },
};

const ECDSA_Config ECDSA_config[CONFIG_ECDSA_COUNT] = {
    {   /* CONFIG_ECDSA_0 */
        .object  = &ecdsaCC26X2Objects[CONFIG_ECDSA_0],
        .hwAttrs = &ecdsaCC26X2HWAttrs[CONFIG_ECDSA_0]
    },
};

const uint_least8_t CONFIG_ECDSA_0_CONST = CONFIG_ECDSA_0;
const uint_least8_t ECDSA_count = CONFIG_ECDSA_COUNT;

#include <ti/drivers/Board.h>

/*
 *  ======== Board_initHook ========
 *  Perform any board-specific initialization needed at startup.  This
 *  function is declared weak to allow applications to override it if needed.
 */
void __attribute__((weak)) Board_initHook(void) {}

/*
 *  ======== Board_init ========
 *  Perform any initialization needed before using any board APIs
 */
void Board_init(void) {
    /* ==== /ti/drivers/Power initialization ==== */
    Power_init();

    /* ==== /ti/devices/CCFG initialization ==== */

    /* ==== /ti/drivers/GPIO initialization ==== */
    /* Setup GPIO module and default-initialise pins */
    GPIO_init();

    /* ==== /ti/drivers/RF initialization ==== */

    Board_shutDownExtFlash();

    Board_initHook();
}
