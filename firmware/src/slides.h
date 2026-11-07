#pragma once
#include <stdint.h>

/* Slide storage in the internal flash, BLE command handler and the slideshow timer
 * for the Hanshow Nebular Pro 154 (200 x 200, BWRY).
 *
 * Flash map (512 KB chip):
 *   0x00000 - 0x13FFF  firmware (the image must stay below 0x14000: firmware/check_size.sh in CI)
 *   0x14000 - 0x75FFF  slides (98 sectors of 4 KB)
 *   0x76000 - 0x77FFF  reserved: MAC address, calibration (do not touch)
 *   0x79000            slideshow settings
 *
 * One slide = one record that starts at a sector boundary:
 *   16 byte header + payload, payload = stored image (raw or LZ4, 2 bpp or 1 bpp).
 */

#define SL_FW_VERSION 26
#define SL_WRITE_MAX  17 /* data bytes the tag takes in one command 0x30 (a write is at most 20 bytes: command, offset(2), data) */

void slides_init(void);
int  slides_low_battery(void);                             /* 1 = battery low: writes and refreshes are stopped */
void slides_tick(void);                                    /* call from the main loop */
int  slides_ble_cmd(const uint8_t *payload, unsigned int len); /* commands 0x20 .. 0x2F */
