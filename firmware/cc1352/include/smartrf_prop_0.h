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
extern RF_Mode Prop0_modeSub1g;

extern rfc_CMD_PROP_RADIO_DIV_SETUP_PA_t Prop0_cmdPropRadioDivSetup;
extern rfc_CMD_FS_t Prop0_cmdFs;
extern rfc_CMD_PROP_TX_t Prop0_cmdPropTx;
extern rfc_CMD_PROP_RX_t Prop0_cmdPropRx;

extern uint32_t Prop0_pOverrides[];
extern uint32_t Prop0_pOverridesTxStd[];
extern uint32_t Prop0_pOverridesTx20[];

/* Band-specific overrides */
extern uint32_t Prop0_pOverrides433[];
extern uint32_t Prop0_pOverrides433TxStd[];
extern uint32_t Prop0_pOverrides433Tx20[];
extern uint32_t Prop0_pOverrides169[];

/* OOK mode and overrides */
extern RF_Mode Prop0_modeOok;
extern uint32_t Prop0_pOverridesOok[];
extern uint32_t Prop0_pOverridesOokTxStd[];
extern uint32_t Prop0_pOverridesOokTx20[];

#endif /* SMARTRF_PROP_0_H */
