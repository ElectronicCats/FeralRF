/* FeralRF CC1352 - GATT attribute table (F20.a.1).
 * Static T2 layout: GAP service + custom test service.
 * Read-only at A3.1; F20.a.2 will add Write/Notify perms. */
#include "gatt_table.h"

static const uint8_t s_gap_uuid[2] = {0x00, 0x18};
static const uint8_t s_dev_name[10] = {'F', 'E', 'R', 'A', 'L', '_', 'G', 'A', 'T', 'T'};
/* CHARACTERISTIC declaration: prop=Read, val_handle=0x0003, UUID=0x2A00 */
static const uint8_t s_devname_char[5] = {0x02, 0x03, 0x00, 0x00, 0x2A};
static const uint8_t s_custom_uuid[2] = {0xE0, 0xFF};
static const uint8_t s_test_value[11] = {'H', 'E', 'L', 'L', 'O', '_', 'F', 'E', 'R', 'A', 'L'};
/* CHARACTERISTIC declaration: prop=Read, val_handle=0x0006, UUID=0xFFE1 */
static const uint8_t s_test_char[5] = {0x02, 0x06, 0x00, 0xE1, 0xFF};

const Attribute g_gatt_table[GATT_TABLE_NUM_ENTRIES] = {
    {0x0001, ATTR_PRIMARY_SERVICE, GATT_PERM_READ, 2, s_gap_uuid},
    {0x0002, ATTR_CHARACTERISTIC, GATT_PERM_READ, 5, s_devname_char},
    {0x0003, 0x2A00, GATT_PERM_READ, 10, s_dev_name},
    {0x0004, ATTR_PRIMARY_SERVICE, GATT_PERM_READ, 2, s_custom_uuid},
    {0x0005, ATTR_CHARACTERISTIC, GATT_PERM_READ, 5, s_test_char},
    {0x0006, 0xFFE1, GATT_PERM_READ, 11, s_test_value},
};

const Attribute *GattTable_findByHandle(uint16_t handle) {
    for (size_t i = 0; i < GATT_TABLE_SIZE; i++) {
        if (g_gatt_table[i].handle == handle) {
            return &g_gatt_table[i];
        }
    }
    return NULL;
}
