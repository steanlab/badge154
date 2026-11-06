#pragma once
#include <stdint.h>

/* Slide storage in the internal flash, BLE command handler and the slideshow timer
 * for the Hanshow Nebular Pro 154 (200 x 200, BWRY).
 *
 * Flash map (512 KB chip):
 *   0x00000 - 0x1FFFF  firmware
 *   0x20000 - 0x75FFF  slides (86 sectors of 4 KB)
 *   0x76000 - 0x77FFF  reserved: MAC address, calibration (do not touch)
 *   0x79000            slideshow settings
 *
 * One slide = one record that starts at a sector boundary:
 *   16 byte header + payload, payload = stored image (raw or LZ4, 2 bpp or 1 bpp).
 */

#define SL_FW_VERSION 11

void slides_init(void);
void slides_tick(void);                                    /* call from the main loop */
int  slides_ble_cmd(const uint8_t *payload, unsigned int len); /* commands 0x20 .. 0x2F */
