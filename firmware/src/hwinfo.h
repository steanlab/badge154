#pragma once
#include <stdint.h>

/* Hardware report of the tag (BLE command 0x2C <page>), see docs/HARDWARE.md.
 * Answer: byte0 0x2C, byte1 result, byte2 page, bytes 3..18 data (little endian), byte19 0x5A.
 *   page 0  live:    3-4 battery mV, 5-8 uptime s, 9-12 seconds to the next slide change (0xFFFFFFFF = none),
 *                    13 TX power dBm (signed), 14-15 ATT MTU, 16-17 connection interval (1.25 ms units, 0 = not connected)
 *   page 1  chip:    3-5 flash JEDEC id (manufacturer, type, capacity code), 6-7 chip temperature sensor raw,
 *                    8-9 panel temperature bytes, 10-13 age of that reading in s (0xFFFFFFFF = none), 14-17 firmware size
 *   page 2  memory:  3-6 RAM used by static data, 7-10 RAM left (for the stack), 11 slide blocks total, 12 free, 13 slides,
 *                    14-15 auto-change interval (min)
 *   page 3  private: 3-18 unique id of the flash chip
 *   page 4  private: 3-8 full Bluetooth MAC address */
typedef struct
{
    uint32_t next_s;        /* seconds to the next automatic change, 0xFFFFFFFF = none */
    uint8_t  sectors;       /* slide blocks in total */
    uint8_t  free_sectors;
    uint8_t  count;         /* slides stored */
    uint16_t interval_min;
} hw_slides_t;

void hwinfo_fill(uint8_t page, uint8_t *r, const hw_slides_t *s);
