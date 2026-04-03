#ifndef SMARTRF_PROP_0_H
#define SMARTRF_PROP_0_H

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_prop_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
/* clang-format on */
#include <ti/drivers/rf/RF.h>

extern RF_Mode Prop0_mode;

extern rfc_CMD_PROP_RADIO_DIV_SETUP_PA_t Prop0_cmdPropRadioDivSetup;
extern rfc_CMD_FS_t Prop0_cmdFs;
extern rfc_CMD_PROP_TX_t Prop0_cmdPropTx;
extern rfc_CMD_PROP_RX_t Prop0_cmdPropRx;

extern uint32_t Prop0_pOverrides[];
extern uint32_t Prop0_pOverridesTxStd[];
extern uint32_t Prop0_pOverridesTx20[];

#endif /* SMARTRF_PROP_0_H */
