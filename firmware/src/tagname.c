#include <stdint.h>
#include "tl_common.h"
#include "stack/ble/ble.h"
#include "tagname.h"

/* ble_name (ble.c) is the scan response: { length, 0x09 (complete local name), "badge154-", name... }.
 * The tail of the name starts at index 11 (the default there is the hex MAC suffix of 6 characters). */
extern uint8_t ble_name[];
#define NAME_AT 11

RAM static uint8_t def_suffix[6];
RAM static uint8_t def_saved = 0;

void tagname_apply(const uint8_t *name, uint8_t len)
{
    uint8_t i;
    if (!def_saved)
    {
        for (i = 0; i < 6; i++)
            def_suffix[i] = ble_name[NAME_AT + i]; /* the MAC suffix written by init_ble() */
        def_saved = 1;
    }
    if (len > TAGNAME_MAX)
        len = TAGNAME_MAX;
    if (len == 0)
    {
        for (i = 0; i < 6; i++)
            ble_name[NAME_AT + i] = def_suffix[i];
        len = 6;
    }
    else
    {
        for (i = 0; i < len; i++)
            ble_name[NAME_AT + i] = name[i];
    }
    ble_name[0] = (uint8_t)(1 + 9 + len); /* type + "badge154-" + name */
    bls_ll_setScanRspData((uint8_t *)ble_name, (uint8_t)(1 + ble_name[0]));
}
