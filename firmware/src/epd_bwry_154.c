#include <stdint.h>
#include "tl_common.h"
#include "main.h"
#include "epd.h"
#include "epd_spi.h"
#include "epd_bwry_154.h"
#include "drivers.h"
#include "stack/ble/ble.h"

/* Driver for the 1.54" 200 x 200 four-colour (black / white / yellow / red) panel of the
 * Hanshow Nebular Pro 154. See docs/HARDWARE.md for what is verified and what is not.
 *
 * The whole sequence (pin handling, power-on, reset timing, init data, data stream, refresh,
 * sleep) follows the official ATC_BLE_OEPL firmware for the preset "38: 200 HS BWRY JD":
 *
 *   ENABLE low (output) = panel power on, 5 ms, RESET low 20 ms / high 20 ms, 40 ms,
 *   wait BUSY high (<=400 ms), init commands, 04 + wait BUSY, 10 + pixel data, 12 00,
 *   wait BUSY high = done, 02 00, wait BUSY, 07 A5, power off.
 *
 * Diagnostic BLE command (EPD_BLE characteristic):   10  <pattern> <reserved> <flags>
 *   pattern: 0..3 = whole screen filled with 2-bit code 0..3
 *            4    = 4 horizontal bands, codes 0,1,2,3 from top to bottom
 *            5    = 4 vertical bands,   codes 0,1,2,3 from left to right
 *            6    = orientation test: white screen, 50x50 squares of code 0 (top left),
 *                   code 2 (top right), code 3 (bottom left)
 *   reserved: 0
 *   flags:   bit0 = skip reading the two bytes after command 40 (temperature)
 *            bit1 = old power method: only switch the ENABLE pull resistor (no output drive)
 *            bit2 = BUSY is active-high (default: BUSY high = idle, as in the official firmware)
 *
 * LED feedback after the init sequence (before the pixel data is sent):
 *   one green blink            = all BUSY waits ended by themselves (panel answers)
 *   N red blinks (N = 1..6)    = N BUSY waits ran into their timeout
 */

#define BW 200
#define BH 200
#define BYTES_PER_ROW (BW / 4)

extern uint8_t epd_update_state;

RAM static uint32_t bwry_refresh_start = 0;
RAM static uint8_t  bwry_busy_active_high = 0;
RAM static uint8_t  bwry_timeouts = 0;

/* default: BUSY high = idle, BUSY low = busy (JD controller) */
static _attribute_ram_code_ uint8_t bwry_is_busy(void)
{
    return bwry_busy_active_high ? gpio_read(EPD_BUSY) : !gpio_read(EPD_BUSY);
}

/* wait until BUSY says idle, at most max_ms; like the official wait function: 1 ms first */
static _attribute_ram_code_ void bwry_wait(uint32_t max_ms)
{
    uint32_t t0 = clock_time();
    WaitMs(1);
    while (bwry_is_busy())
    {
        if ((uint32_t)(clock_time() - t0) > max_ms * CLOCK_16M_SYS_TIMER_CLK_1MS)
        {
            if (bwry_timeouts < 6)
                bwry_timeouts++;
            return;
        }
    }
}

static _attribute_ram_code_ void bwry_cmd(uint8_t cmd, const uint8_t *d, uint8_t n)
{
    uint8_t i;
    EPD_WriteCmd(cmd);
    for (i = 0; i < n; i++)
        EPD_WriteData(d[i]);
}

#define CMD(c, ...)                                  \
    do                                               \
    {                                                \
        static const uint8_t _d[] = {0, ##__VA_ARGS__}; \
        bwry_cmd((c), _d + 1, sizeof(_d) - 1);       \
    } while (0)

/* same bit order as the official read routine: DC high, CS low, 8 x (CLK low, CLK high, sample) */
static _attribute_ram_code_ uint8_t bwry_read_byte(void)
{
    uint8_t i, v = 0;

    gpio_write(EPD_MOSI, 1);
    gpio_set_output_en(EPD_MOSI, 0);
    gpio_set_input_en(EPD_MOSI, 1);
    gpio_write(EPD_DC, 1);
    gpio_write(EPD_CS, 0);
    WaitUs(10);
    for (i = 0; i < 8; i++)
    {
        gpio_write(EPD_CLK, 0);
        gpio_write(EPD_CLK, 1);
        v <<= 1;
        if (gpio_read(EPD_MOSI))
            v |= 1;
    }
    gpio_set_input_en(EPD_MOSI, 0);
    gpio_set_output_en(EPD_MOSI, 1);
    gpio_write(EPD_CS, 1);
    return v;
}

static _attribute_ram_code_ void bwry_blink(uint8_t color, uint8_t times)
{
    uint8_t i;
    for (i = 0; i < times; i++)
    {
        set_led_color(color);
        WaitMs(200);
        set_led_color(0);
        WaitMs(200);
    }
}

/* power on + hardware reset, as the official EPD_Display_start */
static _attribute_ram_code_ void bwry_power_and_reset(uint8_t flags)
{
    EPD_init(); /* all signal pins as GPIO, ENABLE = input with pullup (= panel off) */

    gpio_write(EPD_CS, 1);
    gpio_write(EPD_DC, 1);
    gpio_write(EPD_CLK, 1);
    gpio_write(EPD_RESET, 1);

    WaitMs(5);
    if (flags & 0x02)
        gpio_setup_up_down_resistor(EPD_ENABLE, PM_PIN_PULLDOWN_100K);
    else
        EPD_POWER_ON(); /* ENABLE driven low = panel power on */
    WaitMs(5);

    gpio_write(EPD_RESET, 0);
    WaitMs(20);
    gpio_write(EPD_RESET, 1);
    WaitMs(20);
}

/* init data of the preset "200 HS BWRY JD"; ends with command 10 (start of the pixel data) */
static _attribute_ram_code_ void bwry_init_jd200(uint8_t flags)
{
    WaitMs(40);
    bwry_wait(400);

    CMD(0x00, 0x0F, 0x29);
    CMD(0x01, 0x07, 0x00, 0x22, 0x78, 0x0A, 0x22);
    CMD(0x03, 0x10, 0x54, 0x44);
    CMD(0x06, 0xC0, 0xC0, 0xC0);
    CMD(0x41, 0x00);
    CMD(0x50, 0x37);
    CMD(0x60, 0x02, 0x02);
    CMD(0xE7, 0x1C);
    CMD(0xE3, 0x22);
    CMD(0xE9, 0x01);
    CMD(0x41, 0x00);
    CMD(0x40); /* temperature sensor */
    bwry_wait(400);
    if (!(flags & 0x01))
    {
        (void)bwry_read_byte();
        (void)bwry_read_byte();
    }
    CMD(0xFF, 0xA5);
    CMD(0xEF, 0x03, 0x44, 0x07, 0x32, 0x07, 0x16, 0x09, 0x33);
    CMD(0xDC, 0x01);
    CMD(0xDD, 0x06);
    CMD(0xDE, 0x70);
    CMD(0xE8, 0x00);
    CMD(0xDA, 0x00);
    CMD(0xFF, 0xE3);
    CMD(0x61, 0x00, 0xC8, 0x00, 0xC8); /* 200 x 200 */
    CMD(0x65, 0x00, 0x00, 0x00, 0x00); /* window offset */
    CMD(0x04);                          /* power on */
    bwry_wait(400);
    EPD_WriteCmd(0x10); /* data start */
}

/* 2-bit code of the pixel at (x, y) for the test patterns */
static _attribute_ram_code_ uint8_t bwry_pat_code(uint8_t pattern, int x, int y)
{
    uint8_t c;
    switch (pattern)
    {
    case 4:
        c = y / 50;
        break;
    case 5:
        c = x / 50;
        break;
    case 6:
        c = 1;
        if (y < 50 && x < 50)
            c = 0;
        else if (y < 50 && x >= BW - 50)
            c = 2;
        else if (y >= BH - 50 && x < 50)
            c = 3;
        break;
    default:
        c = pattern & 3;
        break;
    }
    return c > 3 ? 3 : c;
}

/* The 1-bpp buffer of this firmware is column-major: image[x * 25 + y / 8], bit 7 = top. */
static _attribute_ram_code_ uint8_t bwry_img_code(const unsigned char *image, int x, int y)
{
    uint8_t bit = (image[x * (BH / 8) + (y >> 3)] >> (7 - (y & 7))) & 1;
    return bit ? 0x01 : 0x00; /* white : black */
}

static _attribute_ram_code_ void bwry_run(uint8_t pattern, uint8_t flags, const unsigned char *image)
{
    int x, y, b;

    bwry_busy_active_high = (flags & 0x04) ? 1 : 0;
    bwry_timeouts = 0;

    bwry_power_and_reset(flags);
    bwry_init_jd200(flags);

    if (bwry_timeouts == 0)
        bwry_blink(2, 1);
    else
        bwry_blink(1, bwry_timeouts);

    for (y = 0; y < BH; y++)
    {
        for (b = 0; b < BYTES_PER_ROW; b++)
        {
            uint8_t v = 0;
            for (x = b * 4; x < b * 4 + 4; x++)
            {
                uint8_t c = image ? bwry_img_code(image, x, y) : bwry_pat_code(pattern, x, y);
                v = (uint8_t)((v << 2) | c);
            }
            EPD_WriteData(v);
        }
    }

    CMD(0x12, 0x00); /* refresh */
    bwry_refresh_start = clock_time();
}

_attribute_ram_code_ uint8_t EPD_BWRY_154_Display(unsigned char *image, int size, uint8_t full_or_partial)
{
    (void)size;
    (void)full_or_partial;
    bwry_run(0, 0, image);
    return 20;
}

/* Shows a finished 200 x 200 image: 2 bits per pixel, 4 pixels per byte, MSB first, row by row
 * (10000 bytes; codes 0 = black, 1 = white, 2 = yellow, 3 = red). Returns at once after the
 * refresh command; epd_state_handler() puts the panel to sleep when BUSY says it is done. */
_attribute_ram_code_ void EPD_BWRY_154_show(const uint8_t *img)
{
    uint16_t i;

    if (epd_update_state)
        return;

    bwry_busy_active_high = 0;
    bwry_timeouts = 0;
    bwry_power_and_reset(0);
    bwry_init_jd200(0);
    for (i = 0; i < BW * BH / 4; i++)
        EPD_WriteData(img[i]);
    CMD(0x12, 0x00); /* refresh */
    bwry_refresh_start = clock_time();
    epd_update_state = 1;
}

_attribute_ram_code_ void EPD_BWRY_154_test(uint8_t pattern, uint8_t reserved, uint8_t flags)
{
    (void)reserved;
    if (epd_update_state)
        return; /* previous refresh still running */

    bwry_run(pattern, flags, (const unsigned char *)0);
    epd_update_state = 1;
}

_attribute_ram_code_ uint8_t EPD_BWRY_154_done(void)
{
    uint32_t el = (uint32_t)(clock_time() - bwry_refresh_start);
    if (el > 90u * CLOCK_16M_SYS_TIMER_CLK_1MS * 1000u)
        return 1; /* safety timeout */
    if (el < 3u * CLOCK_16M_SYS_TIMER_CLK_1MS * 1000u)
        return 0; /* refresh just started, BUSY may not have changed yet */
    return !bwry_is_busy();
}

_attribute_ram_code_ void EPD_BWRY_154_set_sleep(void)
{
    CMD(0x02, 0x00); /* power off */
    bwry_wait(400);
    CMD(0x07, 0xA5); /* deep sleep */
    EPD_POWER_OFF();
}
