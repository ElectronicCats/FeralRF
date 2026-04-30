/*
 *  ======== ti_drivers_config.h ========
 *  Configured TI-Drivers module declarations
 *
 *  The macros defines herein are intended for use by applications which
 *  directly include this header. These macros should NOT be hard coded or
 *  copied into library source code.
 *
 *  Symbols declared as const are intended for use with libraries.
 *  Library source code must extern the correct symbol--which is resolved
 *  when the application is linked.
 *
 *  DO NOT EDIT - This file is generated for the LP_CC1352P7_4
 *  by the SysConfig tool.
 */
#ifndef ti_drivers_config_h
#define ti_drivers_config_h

#define CONFIG_SYSCONFIG_PREVIEW

#define CONFIG_LP_CC1352P7_4
#ifndef DeviceFamily_CC13X2X7
#define DeviceFamily_CC13X2X7
#endif

#include <ti/devices/DeviceFamily.h>

#include <stdint.h>

/* support C++ sources */
#ifdef __cplusplus
extern "C" {
#endif


/*
 *  ======== CCFG ========
 */


/*
 *  ======== GPIO ========
 */
/* Owned by /ti/drivers/RF as  */
extern const uint_least8_t CONFIG_RF_24GHZ_CONST;
#define CONFIG_RF_24GHZ 28

/* Owned by /ti/drivers/RF as  */
extern const uint_least8_t CONFIG_RF_HIGH_PA_CONST;
#define CONFIG_RF_HIGH_PA 29

/* Owned by /ti/drivers/RF as  */
extern const uint_least8_t CONFIG_RF_SUB1GHZ_CONST;
#define CONFIG_RF_SUB1GHZ 30

extern const uint_least8_t CONFIG_GPIO_GLED_CONST;
#define CONFIG_GPIO_GLED 7

/* The range of pins available on this device */
extern const uint_least8_t GPIO_pinLowerBound;
extern const uint_least8_t GPIO_pinUpperBound;

/* LEDs are active high */
#define CONFIG_GPIO_LED_ON  (1)
#define CONFIG_GPIO_LED_OFF (0)

#define CONFIG_LED_ON  (CONFIG_GPIO_LED_ON)
#define CONFIG_LED_OFF (CONFIG_GPIO_LED_OFF)


/*
 *  ======== RF ========
 */
#define Board_DIO_30_RFSW 0x0000001e


/*
 *  ======== TRNG ========
 */
extern const uint_least8_t CONFIG_TRNG_0_CONST;
#define CONFIG_TRNG_0 0
#define CONFIG_TRNG_COUNT 1

/*
 *  ======== AESECB ========
 */
extern const uint_least8_t CONFIG_AESECB_0_CONST;
#define CONFIG_AESECB_0 0
#define CONFIG_AESECB_COUNT 1

/*
 *  ======== AESCCM ========
 */
extern const uint_least8_t CONFIG_AESCCM_0_CONST;
#define CONFIG_AESCCM_0 0
#define CONFIG_AESCCM_COUNT 1

/*
 *  ======== AESCTR ========
 */
extern const uint_least8_t CONFIG_AESCTR_0_CONST;
#define CONFIG_AESCTR_0 0
#define CONFIG_AESCTR_COUNT 1

/*
 *  ======== AESCBC ========
 */
extern const uint_least8_t CONFIG_AESCBC_0_CONST;
#define CONFIG_AESCBC_0 0
#define CONFIG_AESCBC_COUNT 1

/*
 *  ======== AESGCM ========
 */
extern const uint_least8_t CONFIG_AESGCM_0_CONST;
#define CONFIG_AESGCM_0 0
#define CONFIG_AESGCM_COUNT 1

/*
 *  ======== SHA2 ========
 */
extern const uint_least8_t CONFIG_SHA2_0_CONST;
#define CONFIG_SHA2_0 0
#define CONFIG_SHA2_COUNT 1

/*
 *  ======== ECDH ========
 */
extern const uint_least8_t CONFIG_ECDH_0_CONST;
#define CONFIG_ECDH_0 0
#define CONFIG_ECDH_COUNT 1

/*
 *  ======== ECDSA ========
 */
extern const uint_least8_t CONFIG_ECDSA_0_CONST;
#define CONFIG_ECDSA_0 0
#define CONFIG_ECDSA_COUNT 1


/*
 *  ======== Board_init ========
 *  Perform all required TI-Drivers initialization
 *
 *  This function should be called once at a point before any use of
 *  TI-Drivers.
 */
extern void Board_init(void);

/*
 *  ======== Board_initGeneral ========
 *  (deprecated)
 *
 *  Board_initGeneral() is defined purely for backward compatibility.
 *
 *  All new code should use Board_init() to do any required TI-Drivers
 *  initialization _and_ use <Driver>_init() for only where specific drivers
 *  are explicitly referenced by the application.  <Driver>_init() functions
 *  are idempotent.
 */
#define Board_initGeneral Board_init

#ifdef __cplusplus
}
#endif

#endif /* include guard */
