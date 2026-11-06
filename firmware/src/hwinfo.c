#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "drivers/8258/flash.h"
#include "drivers/8258/spi_i.h"
#include "battery.h"
#include "epd_bwry_154.h"
#include "time.h"
#include "hwinfo.h"

extern uint8_t mac_public[6];
extern uint32_t _end_bss_; /* linker symbol: end of the static RAM; the stack grows down from 0x850000 */

#define SRAM_START 0x840000u
#define SRAM_END   0x850000u
#define TX_DBM     3 /* ble.c: rf_set_power_level_index(RF_POWER_P3p01dBm); change both together */

/* The flash driver of this SDK has no id functions, so the JEDEC id (0x9F) and the unique id (0x4B, XTX: 0x5A) are read here.
 * Code that talks to the flash chip must run from RAM and with interrupts off, like the driver does. */
static _attribute_ram_code_ void fl_cmd(uint8_t cmd)
{
    mspi_high();
    sleep_us(1);
    mspi_low();
    mspi_write(cmd);
    mspi_wait();
}

static _attribute_ram_code_ void fl_read_mid(uint8_t *mid)
{
    unsigned char r = irq_disable();
    fl_cmd(0x9F);
    mid[0] = mspi_read();
    mid[1] = mspi_read();
    mid[2] = mspi_read();
    mspi_high();
    irq_restore(r);
}

static _attribute_ram_code_ void fl_read_uid(uint8_t cmd, uint8_t *uid)
{
    unsigned char r = irq_disable();
    uint8_t i;
    fl_cmd(cmd);
    for (i = 0; i < 4; i++) /* GD / Puya: 4 dummy bytes; XTX: 3 address bytes (0) and 1 dummy */
    {
        mspi_write(0);
        mspi_wait();
    }
    for (i = 0; i < 16; i++)
        uid[i] = mspi_read();
    mspi_high();
    irq_restore(r);
}

static void put16(uint8_t *r, uint16_t v)
{
    r[0] = (uint8_t)v;
    r[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *r, uint32_t v)
{
    put16(r, (uint16_t)v);
    put16(r + 2, (uint16_t)(v >> 16));
}

void hwinfo_fill(uint8_t page, uint8_t *r, const hw_slides_t *s)
{
    uint8_t i;
    uint8_t *d = r + 3;

    for (i = 0; i < 20; i++)
        r[i] = 0;
    r[0] = 0x2C;
    r[2] = page;
    r[19] = 0x5A;

    switch (page)
    {
    case 0:
        put16(d, get_battery_mv());
        put32(d + 2, get_time());
        put32(d + 6, s->next_s);
        d[10] = (uint8_t)(int8_t)TX_DBM;
        put16(d + 11, blc_att_getEffectiveMtuSize(0));
        put16(d + 13, bls_ll_getConnectionInterval());
        break;
    case 1:
    {
        uint8_t mid[4] = {0, 0, 0, 0};
        uint8_t pt[2];
        uint8_t hdr[4] = {0, 0, 0, 0};
        uint32_t age;
        fl_read_mid(mid);
        d[0] = mid[0];
        d[1] = mid[1];
        d[2] = mid[2];
        put16(d + 3, get_temperature_c());
        age = EPD_BWRY_154_get_temp(pt);
        d[5] = pt[0];
        d[6] = pt[1];
        put32(d + 7, age);
        flash_read_page(0x18, 4, hdr); /* the image header holds the firmware size */
        put32(d + 11, (uint32_t)hdr[0] | ((uint32_t)hdr[1] << 8) | ((uint32_t)hdr[2] << 16) | ((uint32_t)hdr[3] << 24));
        break;
    }
    case 2:
    {
        uint32_t e = (uint32_t)&_end_bss_;
        put32(d, e - SRAM_START);
        put32(d + 4, SRAM_END - e);
        d[8] = s->sectors;
        d[9] = s->free_sectors;
        d[10] = s->count;
        put16(d + 11, s->interval_min);
        break;
    }
    case 3:
    {
        uint8_t mid[4] = {0, 0, 0, 0};
        uint8_t uid[16];
        fl_read_mid(mid);
        fl_read_uid(mid[0] == 0x0B ? 0x5A : 0x4B, uid);
        for (i = 0; i < 16; i++)
            d[i] = uid[i];
        break;
    }
    case 4:
        for (i = 0; i < 6; i++)
            d[i] = mac_public[i];
        break;
    default:
        r[1] = 1;
        break;
    }
}
