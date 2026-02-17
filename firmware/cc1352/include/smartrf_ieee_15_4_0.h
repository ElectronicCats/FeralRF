#ifndef SMARTRF_IEEE_15_4_0_H
#define SMARTRF_IEEE_15_4_0_H

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_ieee_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
/* clang-format on */
#include <ti/drivers/rf/RF.h>

extern RF_Mode Ieee154_0_mode;

extern rfc_CMD_RADIO_SETUP_PA_t Ieee154_0_cmdRadioSetup;
extern rfc_CMD_FS_t Ieee154_0_cmdFs;
extern rfc_CMD_IEEE_RX_t Ieee154_0_cmdIeeeRx;

extern uint32_t Ieee154_0_pOverrides[];

#endif /* SMARTRF_IEEE_15_4_0_H */
