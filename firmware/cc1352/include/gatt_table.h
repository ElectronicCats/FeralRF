/* FeralRF CC1352 - GATT attribute table (F20.a.1).
 * Static T2 layout: GAP service + custom test service.
 * Read-only at A3.1; F20.a.2 will add Write/Notify perms. */
#ifndef GATT_TABLE_H
#define GATT_TABLE_H

#include <stddef.h>
#include <stdint.h>

#define ATTR_PRIMARY_SERVICE 0x2800u
#define ATTR_CHARACTERISTIC 0x2803u

#define GATT_PERM_READ 0x01u
#define GATT_PERM_WRITE 0x02u  /* F20.a.2 */
#define GATT_PERM_NOTIFY 0x10u /* F20.a.2 */

typedef struct {
    uint16_t handle;
    uint16_t type; /* ATTR_* enum or specific UUID16 */
    uint8_t perms;
    uint8_t value_len;
    const uint8_t *value;
} Attribute;

#define GATT_TABLE_NUM_ENTRIES 6u
extern const Attribute g_gatt_table[GATT_TABLE_NUM_ENTRIES];
#define GATT_TABLE_SIZE (sizeof(g_gatt_table) / sizeof(g_gatt_table[0]))

const Attribute *GattTable_findByHandle(uint16_t handle);

#endif /* GATT_TABLE_H */
