#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "main.h"
#include "ble.h"
#include "time.h"
#include "led_link.h"

/* The LED channels are active low (0 = on). Only PB4 and PD3 could use the hardware PWM, the blue channel (PA7) cannot,
 * so all three are driven by a software PWM from the main loop (no sleep while the show runs).
 * PWM: 16 levels, one level = 2048 system ticks (128 us at 16 MHz) -> about 490 Hz.
 * Colour: a full red-green-blue-red cycle takes CYCLE time units (1 unit = 16384 ticks, about 1 ms at 16 MHz). */
#define CYCLE        2400u     /* 3 colours x 800 ms */
#define THIRD        (CYCLE / 3u)
#define SHOW_MAX_S   600u      /* the show stops after 10 minutes of one connection (battery) */

RAM static uint8_t  show_on = 0;       /* the light show is running */
RAM static uint8_t  was_conn = 0;
RAM static uint32_t conn_since = 0;

static uint8_t chan_level(uint32_t pos, uint32_t center)
{
    int32_t d = (int32_t)pos - (int32_t)center;
    uint32_t b;
    if (d < 0)
        d = -d;
    if ((uint32_t)d > CYCLE / 2u)
        d = (int32_t)(CYCLE - (uint32_t)d); /* distance around the circle */
    if ((uint32_t)d >= THIRD)
        return 0;
    b = (THIRD - (uint32_t)d) * 16u / THIRD; /* 0..16, triangle */
    return (uint8_t)((b * b + 8u) >> 4);     /* gamma 2: looks linear to the eye */
}

static void leds_off(void)
{
    gpio_write(LED_RED, 1);
    gpio_write(LED_GREEN, 1);
    gpio_write(LED_BLUE, 1);
}

uint8_t led_link_active(void)
{
    return show_on;
}

void led_link_tick(void)
{
    uint32_t now, pos, ph;
    uint8_t conn = ble_get_connected() ? 1 : 0;

    if (conn && !was_conn)
    {
        conn_since = get_time();
        show_on = 1;
    }
    if (!conn && was_conn)
    {
        show_on = 0;
        leds_off();
    }
    was_conn = conn;
    if (!show_on)
        return;

    if ((uint32_t)(get_time() - conn_since) >= SHOW_MAX_S)
    {
        show_on = 0;
        leds_off();
        return;
    }

    now = clock_time();
    ph = (now >> 11) & 15u;                 /* PWM phase 0..15 */
    pos = (now >> 14) % CYCLE;              /* position in the colour cycle */
    gpio_write(LED_RED, chan_level(pos, 0) > ph ? 0 : 1);
    gpio_write(LED_GREEN, chan_level(pos, THIRD) > ph ? 0 : 1);
    gpio_write(LED_BLUE, chan_level(pos, 2u * THIRD) > ph ? 0 : 1);
}
