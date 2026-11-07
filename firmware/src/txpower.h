#pragma once
#include <stdint.h>

/* Radio output power, selectable (BLE command 0x2F <index>).  The level is stored with the other settings in the flash
 * (slides.c) and applied at every start and after every sleep (ble.c / app.c call txpower_apply()). */
#define TX_LEVELS  9
#define TX_DEFAULT 2 /* +3 dBm, as before the setting existed */

void   txpower_set(uint8_t idx); /* remember and apply; an index out of range is ignored */
void   txpower_apply(void);      /* write the remembered level to the radio */
int8_t txpower_dbm(void);        /* remembered level in dBm, rounded */
