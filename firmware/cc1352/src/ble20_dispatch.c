/* FeralRF CC1352 - L2CAP/ATT RX dispatcher (F20.a.1).
 * Drains s_rf_data_queue post-connection-event, routes ATT frames
 * (L2CAP CID 0x0004) to AttServer, detects LL_TERMINATE_IND for
 * clean disconnect. */
#include "ble20_dispatch.h"

#include "att_server.h"
#include "radio_if.h"

#include <ti/devices/DeviceFamily.h>
/* clang-format off */
#include DeviceFamily_constructPath(driverlib/rf_data_entry.h)
/* clang-format on */

void Ble20_drainAndDispatch(uint8_t *reason_out) {
    if (reason_out)
        *reason_out = 0u;
    dataQueue_t *q = RadioIF_getRxQueue();
    rfc_dataEntryGeneral_t *entry = (rfc_dataEntryGeneral_t *)q->pCurrEntry;
    while (entry != NULL && entry->status == DATA_ENTRY_FINISHED) {
        uint8_t *pkt = (uint8_t *)&entry->data;
        /* LL data PDU header: byte 0 = LLID + flags, byte 1 = length */
        uint8_t llid = pkt[0] & 0x03u;
        uint8_t length = pkt[1];

        if (llid == 0x3u && length >= 1u) {
            /* LL Control PDU — opcode at pkt[2] */
            uint8_t opcode = pkt[2];
            if (opcode == 0x02u && length >= 2u) {
                /* LL_TERMINATE_IND: reason at pkt[3] */
                if (reason_out)
                    *reason_out = pkt[3];
            }
        } else if ((llid == 0x1u || llid == 0x2u) && length >= 4u) {
            /* L2CAP frame: [len:2 LE][cid:2 LE][payload] */
            uint16_t l2_len = (uint16_t)pkt[2] | ((uint16_t)pkt[3] << 8);
            uint16_t l2_cid = (uint16_t)pkt[4] | ((uint16_t)pkt[5] << 8);
            if (l2_cid == 0x0004u && l2_len >= 1u && length >= (uint8_t)(l2_len + 4u)) {
                AttServer_handleRequest(&pkt[6], (uint8_t)l2_len);
            }
        }
        entry->status = DATA_ENTRY_PENDING;
        entry = (rfc_dataEntryGeneral_t *)entry->pNextEntry;
    }
}
