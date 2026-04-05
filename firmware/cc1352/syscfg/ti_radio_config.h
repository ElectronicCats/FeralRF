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
// RF design based on: LP_CC1352P7-1
#define LP_CC1352P7_1

// High-Power Amplifier supported
#define SUPPORT_HIGH_PA

// RF frontend configuration
#define FRONTEND_SUB1G_DIFF_RF
#define FRONTEND_SUB1G_EXT_BIAS
#define FRONTEND_24G_DIFF_RF
#define FRONTEND_24G_EXT_BIAS

// Supported frequency bands
#define SUPPORT_FREQBAND_868
#define SUPPORT_FREQBAND_2400

// TX power table size definitions
#define TXPOWERTABLE_868_PA13_SIZE 20 // 868 MHz, 13 dBm
#define TXPOWERTABLE_868_PA20_SIZE 8 // 868 MHz, 20 dBm
#define TXPOWERTABLE_2400_PA5_SIZE 16 // 2400 MHz, 5 dBm

// TX power tables
extern RF_TxPowerTable_Entry txPowerTable_868_pa13[]; // 868 MHz, 13 dBm
extern RF_TxPowerTable_Entry txPowerTable_868_pa20[]; // 868 MHz, 20 dBm
extern RF_TxPowerTable_Entry txPowerTable_2400_pa5[]; // 2400 MHz, 5 dBm



//*********************************************************************************
//  RF Setting:   BLE, 1 Mbps, LE 1M
//
//  PHY:          bt5le1m
//  Setting file: setting_bt5_le_1m.json
//*********************************************************************************

// Custom override offsets
#define BLE_APP_OVERRIDES_OFFSET 17

// PA table usage
#define TX_POWER_TABLE_SIZE TXPOWERTABLE_2400_PA5_SIZE

#define txPowerTable txPowerTable_2400_pa5

// TI-RTOS RF Mode object
extern RF_Mode RF_prop;

// RF Core API commands
extern rfc_CMD_BLE5_RADIO_SETUP_PA_t RF_cmdBle5RadioSetup;
extern rfc_CMD_FS_t RF_cmdFs;
extern rfc_CMD_BLE5_GENERIC_RX_t RF_cmdBle5GenericRx;
extern rfc_CMD_BLE_ADV_t RF_cmdBleAdv;
extern rfc_CMD_BLE_SCANNER_t RF_cmdBleScanner;
extern rfc_CMD_BLE5_SLAVE_t RF_cmdBle5Slave;
extern rfc_CMD_BLE5_MASTER_t RF_cmdBle5Master;
extern rfc_CMD_BLE5_SCANNER_t RF_cmdBle5Scanner;
extern rfc_CMD_BLE5_INITIATOR_t RF_cmdBle5Initiator;

// RF Core API overrides
extern uint32_t pOverridesCommon[];
extern uint32_t pOverrides1Mbps[];
extern uint32_t pOverrides2Mbps[];
extern uint32_t pOverridesCoded[];

#endif // _TI_RADIO_CONFIG_H_
