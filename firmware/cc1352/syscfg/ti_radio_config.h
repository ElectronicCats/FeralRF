/*
 *  ======== ti_radio_config.h ========
 *  Configured RadioConfig module definitions
 *
 *  DO NOT EDIT - This file is generated for the CC1352P7RGZ
 *  by the SysConfig tool.
 *
 *  Radio Config module version : 1.20.0
 *  SmartRF Studio data version : 2.32.0
 */
#ifndef _TI_RADIO_CONFIG_H_
#define _TI_RADIO_CONFIG_H_

#include <ti/devices/DeviceFamily.h>
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_ble_cmd.h)
#include <ti/drivers/rf/RF.h>

/* SmartRF Studio version that the RF data is fetched from */
#define SMARTRF_STUDIO_VERSION "2.32.0"

// *********************************************************************************
//   RF Frontend configuration
// *********************************************************************************
// RF design based on: LP_CC1352P7-4
#define LP_CC1352P7_4

// High-Power Amplifier supported
#define SUPPORT_HIGH_PA

// RF frontend configuration
#define FRONTEND_24G_DIFF_RF
#define FRONTEND_24G_EXT_BIAS

// Supported frequency bands
#define SUPPORT_FREQBAND_2400

// TX power table size definitions
#define TXPOWERTABLE_2400_PA5_SIZE 16 // 2400 MHz, 5 dBm
#define TXPOWERTABLE_2400_PA5_10_SIZE 21 // 2400 MHz, 5 + 10 dBm

// TX power tables
extern RF_TxPowerTable_Entry txPowerTable_2400_pa5[]; // 2400 MHz, 5 dBm
extern RF_TxPowerTable_Entry txPowerTable_2400_pa5_10[]; // 2400 MHz, 5 + 10 dBm



//*********************************************************************************
//  RF Setting:   BLE, 2 Mbps, LE 2M, 10 dBm
//
//  PHY:          bt5le2mp10
//  Setting file: setting_bt5_le_2m_10_dbm.json
//*********************************************************************************

// Custom override offsets
#define BLE_STACK_OVERRIDES_OFFSET 18

// PA table usage
#define RF_BLE_TX_POWER_TABLE_SIZE TXPOWERTABLE_2400_PA5_10_SIZE

#define RF_BLE_txPowerTable txPowerTable_2400_pa5_10

// TI-RTOS RF Mode object
extern RF_Mode RF_modeBle;

// RF Core API overrides
extern uint32_t pOverrides_bleCommon[];
extern uint32_t pOverrides_ble1Mbps[];
extern uint32_t pOverrides_ble2Mbps[];
extern uint32_t pOverrides_bleCoded[];
extern uint32_t pOverrides_bleTxStd[];
extern uint32_t pOverrides_bleTx20[];

#endif // _TI_RADIO_CONFIG_H_
