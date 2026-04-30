/*
 * FeralRF CC1352 - Proprietary 2.4 GHz SmartRF config
 *
 * Provides RF_Mode + RadioSetup/FS/TX/RX command structs for prop
 * 2.4 GHz operation (CMD_PROP_RADIO_SETUP 0x3806, no loDivider).
 * Default modulation: GFSK 250 kbps, deviation 125 kHz, sync word
 * 0x930B51DE, frequency 2440 MHz. configure_prop() can override
 * symbol_rate/deviation/sync_word at runtime.
 */

#ifndef SMARTRF_PROP_2_4GHZ_H
#define SMARTRF_PROP_2_4GHZ_H

#include <ti/devices/DeviceFamily.h>
#include <ti/drivers/rf/RF.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_prop_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_common_cmd.h)
#include DeviceFamily_constructPath(driverlib/rf_mailbox.h)
/* clang-format on */

extern RF_Mode Prop24g_mode;

extern rfc_CMD_PROP_RADIO_SETUP_t Prop24g_cmdPropRadioSetup;
extern rfc_CMD_FS_t Prop24g_cmdFs;
extern rfc_CMD_PROP_TX_t Prop24g_cmdPropTx;
extern rfc_CMD_PROP_RX_t Prop24g_cmdPropRx;

extern uint32_t Prop24g_pOverrides[];

#endif /* SMARTRF_PROP_2_4GHZ_H */
