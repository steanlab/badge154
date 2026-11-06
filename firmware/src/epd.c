#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bwry_154.h"
#include "drivers.h"

/* badge154: replaces the upstream epd.c (multi-panel support, clock drawing, fonts, TIFF decoder).
 * Slides are drawn by slides.c straight through EPD_BWRY_154_show(); this file only keeps the refresh state. */

RAM uint8_t epd_update_state = 0;

_attribute_ram_code_ uint8_t EPD_read_temp(void)
{
    return 20;
}

_attribute_ram_code_ void epd_set_sleep(void)
{
    EPD_BWRY_154_set_sleep();
    EPD_POWER_OFF();
    epd_update_state = 0;
}

_attribute_ram_code_ uint8_t epd_state_handler(void)
{
    if (epd_update_state == 1 && EPD_BWRY_154_done())
        epd_set_sleep(); /* refresh finished: put the panel to sleep and cut its power */
    return epd_update_state;
}
