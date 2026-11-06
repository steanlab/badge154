#pragma once
#include <stdint.h>

/* 1.54" 200x200 BWRY panel (JD-family 4-colour controller), Hanshow Nebular Pro 154.
 * Sequence taken from the official ATC_BLE_OEPL firmware, preset "38: 200 HS BWRY JD".
 * Pixel format: 2 bits per pixel, 4 pixels per byte, MSB first. */

uint8_t EPD_BWRY_154_Display(unsigned char *image, int size, uint8_t full_or_partial);
void    EPD_BWRY_154_show(const uint8_t *img);
void    EPD_BWRY_154_test(uint8_t pattern, uint8_t reserved, uint8_t flags);
void    EPD_BWRY_154_set_sleep(void);
uint8_t EPD_BWRY_154_done(void);
uint32_t EPD_BWRY_154_get_temp(uint8_t *b2);
