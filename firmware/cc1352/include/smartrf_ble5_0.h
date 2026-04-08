#ifndef SMARTRF_BLE5_0_H
#define SMARTRF_BLE5_0_H

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_ble_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
/* clang-format on */
#include <ti/drivers/rf/RF.h>

extern RF_Mode Ble5_0_mode;

extern rfc_CMD_BLE5_RADIO_SETUP_PA_t Ble5_0_cmdBle5RadioSetup;
extern rfc_CMD_FS_t Ble5_0_cmdFs;
extern rfc_CMD_BLE5_GENERIC_RX_t Ble5_0_cmdBle5GenericRx;
extern rfc_CMD_BLE5_SCANNER_t Ble5_0_cmdBle5Scanner;
extern rfc_CMD_BLE5_ADV_NC_t Ble5_0_cmdBle5AdvNc;
extern rfc_CMD_BLE_ADV_NC_t Ble5_0_cmdBleAdvNc;
extern rfc_CMD_BLE5_ADV_EXT_t Ble5_0_cmdBle5AdvExt;
extern rfc_CMD_BLE5_ADV_AUX_t Ble5_0_cmdBle5AdvAux;

extern uint32_t Ble5_0_pOverridesCommon[];
extern uint32_t Ble5_0_pOverrides1Mbps[];
extern uint32_t Ble5_0_pOverrides2Mbps[];
extern uint32_t Ble5_0_pOverridesCoded[];

#endif /* SMARTRF_BLE5_0_H */
