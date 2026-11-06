#pragma once
#include <stdint.h>

/* badge154: the only panel is the 200 x 200 BWRY panel, driven by epd_bwry_154.c */
#define epd_height 200
#define epd_width 200

extern uint8_t epd_update_state; /* 1 = the panel is refreshing, 0 = idle */

uint8_t EPD_read_temp(void);     /* panel temperature in degrees C (fixed 20 for now) */
void epd_set_sleep(void);        /* panel to sleep, power off */
uint8_t epd_state_handler(void); /* call from the main loop: powers the panel off when the refresh is done */
