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

extern const Attribute g_gatt_table[];
extern const size_t g_gatt_table_size;

const Attribute *GattTable_findByHandle(uint16_t handle);

#endif /* GATT_TABLE_H */
