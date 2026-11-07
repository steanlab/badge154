#include <stdint.h>

#ifdef SLIDES_HOST
#include "host_stubs.h"
#else
#include "tl_common.h"
#include "main.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "epd.h"
#include "epd_bwry_154.h"
#include "time.h"
#include "ble.h"
#include "battery.h"
#endif

#include "slides.h"
#include "splash.h"
#include "nfc_probe.h"
#include "led_link.h"
#include "hwinfo.h"
#include "wdt.h"
#include "textscr.h"
#include "txpower.h"
#include "tagname.h"

/* ---------------------------------------------------------------------------------------------
 * constants
 * ------------------------------------------------------------------------------------------- */
#define SL_BASE       0x14000u
#define SL_SECTORS    98u
#define SL_SECSZ      4096u
#define SL_HDR        16u
#define SL_MAGIC      0x4C53u
#define SL_MAX_SLOTS  120u
#define SL_MAX_LEN    10000u            /* stored payload, never more than the raw 2 bpp image */
#define IMG_BYTES     10000u            /* 200 x 200 x 2 bit */
#define CFG_ADDR      0x79000u
#define CFG_MAGIC     0x534C4431u       /* 'SLD1' */

#define F_LZ4         0x01
#define F_1BPP        0x02

/* result codes (byte 1 of the status) */
#define R_OK          0
#define R_BADARG      1
#define R_NOSPACE     2
#define R_CRC         3
#define R_BUSY        4
#define R_FLASH       5
#define R_NOTFOUND    6
#define R_NOUPLOAD    7
#define R_LOWBAT      8
#define R_UNKNOWN     9

#define NO_SLOT       0xFF

extern uint16_t battery_mv;
/* status buffer behind the readable BLE characteristic (kept in the retention RAM) */
RAM uint8_t my_EPD_BLE_Data[20] = {0};

/* decoded image: 200 x 200, 2 bits per pixel, 4 pixels per byte, MSB first */
static uint8_t slide_img[IMG_BYTES];

/* ---------------------------------------------------------------------------------------------
 * small helpers
 * ------------------------------------------------------------------------------------------- */
static uint16_t crc16_update(uint16_t crc, const uint8_t *d, unsigned int n)
{
    unsigned int i;
    uint8_t b;
    for (i = 0; i < n; i++)
    {
        crc ^= (uint16_t)d[i] << 8;
        for (b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

static void fl_read(uint32_t addr, unsigned int len, uint8_t *buf)
{
    flash_read_page(addr, len, buf);
}

/* write len bytes, split at 256-byte page borders; the target area must be erased.
 * buf must be in RAM, never in the flash (const data): the flash cannot be read while it is being programmed. */
static void fl_write(uint32_t addr, const uint8_t *buf, unsigned int len)
{
    while (len)
    {
        unsigned int n = 256 - (addr & 255);
        if (n > len)
            n = len;
        flash_write_page(addr, n, (uint8_t *)buf);
        addr += n;
        buf += n;
        len -= n;
    }
}

static void fl_erase(uint32_t addr)
{
    flash_erase_sector(addr);
}

/* ---------------------------------------------------------------------------------------------
 * settings
 * ------------------------------------------------------------------------------------------- */
typedef struct
{
    uint16_t interval_min; /* 0 = off */
    uint8_t  order;        /* 0 = in order, 1 = random */
    uint8_t  flags;        /* bit0: NFC tap does not change slides */
    uint8_t  tx;           /* radio power level, index for txpower.c */
    uint8_t  name_len;     /* name of the tag, see tagname.c; 0 = none */
    uint8_t  name[TAGNAME_MAX];
} sl_cfg_t;

RAM static sl_cfg_t cfg = {60, 0, 0, TX_DEFAULT, 0, {0}};

static uint8_t name_xor(const uint8_t *n, uint8_t len)
{
    uint8_t i, c = 0;
    for (i = 0; i < len; i++)
        c ^= n[i];
    return c;
}

static uint8_t cfg_crc(const uint8_t *b)
{
    uint8_t i, c = 0x5A;
    for (i = 0; i < 12; i++)
        c ^= b[i];
    return c;
}

static void cfg_load(void)
{
    uint8_t b[32];
    fl_read(CFG_ADDR, 32, b);
    if (b[0] == (CFG_MAGIC & 0xFF) && b[1] == ((CFG_MAGIC >> 8) & 0xFF) && b[2] == ((CFG_MAGIC >> 16) & 0xFF) &&
        b[3] == ((CFG_MAGIC >> 24) & 0xFF) && b[12] == cfg_crc(b))
    {
        cfg.interval_min = (uint16_t)(b[4] | (b[5] << 8));
        cfg.order = b[6] & 1;
        cfg.flags = b[7];
        if (b[8] >= 1 && b[8] <= TX_LEVELS) /* 0 = never set (also in the older settings): default */
            cfg.tx = (uint8_t)(b[8] - 1);
        /* b[9] = length of the name (0 in the older settings), b[10] = xor of its bytes, the name is at 16..31 */
        if (b[9] <= TAGNAME_MAX && b[10] == name_xor(b + 16, b[9]))
        {
            cfg.name_len = b[9];
            memcpy(cfg.name, b + 16, b[9]);
        }
        else
        {
            cfg.name_len = 0; /* damaged: the default name */
        }
    }
}

static void cfg_save(void)
{
    uint8_t b[32];
    memset(b, 0xFF, sizeof(b));
    b[0] = CFG_MAGIC & 0xFF;
    b[1] = (CFG_MAGIC >> 8) & 0xFF;
    b[2] = (CFG_MAGIC >> 16) & 0xFF;
    b[3] = (CFG_MAGIC >> 24) & 0xFF;
    b[4] = cfg.interval_min & 0xFF;
    b[5] = cfg.interval_min >> 8;
    b[6] = cfg.order;
    b[7] = cfg.flags;
    b[8] = (uint8_t)(cfg.tx + 1);
    b[9] = cfg.name_len;
    b[10] = name_xor(cfg.name, cfg.name_len);
    b[11] = 0;
    memcpy(b + 16, cfg.name, cfg.name_len);
    b[12] = cfg_crc(b);
    fl_erase(CFG_ADDR);
    fl_write(CFG_ADDR, b, 32);
}

/* ---------------------------------------------------------------------------------------------
 * slide directory (rebuilt from the headers in the flash)
 * ------------------------------------------------------------------------------------------- */
RAM static uint8_t  sec_used[SL_SECTORS] = {0};
RAM static uint8_t  slot_start[SL_MAX_SLOTS] = {0}; /* first sector or NO_SLOT */
RAM static uint8_t  slot_nsec[SL_MAX_SLOTS] = {0};
RAM static uint8_t  playlist[SL_MAX_SLOTS] = {0};   /* slot numbers in ascending order */
RAM static uint8_t  pl_count = 0;

static uint8_t hdr_chk(const uint8_t *h)
{
    uint8_t i, c = 0xA5;
    for (i = 0; i < 10; i++)
        c ^= h[i];
    return c;
}

/* header layout: 0-1 magic, 2 slot, 3 flags, 4-5 length, 6-7 crc16, 8 pal0, 9 pal1, 10 check, 11-15 0xFF */
static int hdr_valid(const uint8_t *h)
{
    uint16_t len = (uint16_t)(h[4] | (h[5] << 8));
    if (h[0] != (SL_MAGIC & 0xFF) || h[1] != (SL_MAGIC >> 8))
        return 0;
    if (h[2] >= SL_MAX_SLOTS || len == 0 || len > SL_MAX_LEN || (h[3] & ~(F_LZ4 | F_1BPP)))
        return 0;
    return h[10] == hdr_chk(h);
}

static unsigned int sectors_for(unsigned int len)
{
    return (SL_HDR + len + SL_SECSZ - 1) / SL_SECSZ;
}

static void sl_scan(void)
{
    uint8_t h[16];
    unsigned int s, i, n, len;

    memset(sec_used, 0, sizeof(sec_used));
    memset(slot_start, NO_SLOT, sizeof(slot_start));
    memset(slot_nsec, 0, sizeof(slot_nsec));

    for (s = 0; s < SL_SECTORS;)
    {
        fl_read(SL_BASE + s * SL_SECSZ, 16, h);
        if (hdr_valid(h))
        {
            len = (unsigned int)(h[4] | (h[5] << 8));
            n = sectors_for(len);
            if (s + n <= SL_SECTORS && slot_start[h[2]] == NO_SLOT)
            {
                slot_start[h[2]] = (uint8_t)s;
                slot_nsec[h[2]] = (uint8_t)n;
                for (i = 0; i < n; i++)
                    sec_used[s + i] = 1;
                s += n;
                continue;
            }
        }
        s++;
    }

    pl_count = 0;
    for (i = 0; i < SL_MAX_SLOTS; i++)
        if (slot_start[i] != NO_SLOT)
            playlist[pl_count++] = (uint8_t)i;
}

static unsigned int free_sectors(void)
{
    unsigned int s, n = 0;
    for (s = 0; s < SL_SECTORS; s++)
        if (!sec_used[s])
            n++;
    return n;
}

static int find_run(unsigned int need)
{
    unsigned int s, run = 0;
    for (s = 0; s < SL_SECTORS; s++)
    {
        if (sec_used[s])
            run = 0;
        else if (++run == need)
            return (int)(s + 1 - need);
    }
    return -1;
}

static void slot_delete(uint8_t slot)
{
    uint8_t zero[2]; /* must be in RAM: the flash cannot be read (XIP) while it is being programmed */
    zero[0] = 0;
    zero[1] = 0;
    if (slot < SL_MAX_SLOTS && slot_start[slot] != NO_SLOT)
    {
        fl_write(SL_BASE + (uint32_t)slot_start[slot] * SL_SECSZ, zero, 2); /* magic -> 0: no longer a slide */
        sl_scan();
    }
}

/* ---------------------------------------------------------------------------------------------
 * upload state
 * ------------------------------------------------------------------------------------------- */
RAM static uint8_t  up_active = 0;
RAM static uint8_t  up_slot = 0, up_flags = 0, up_pal0 = 0, up_pal1 = 0;
RAM static uint16_t up_len = 0, up_crc = 0, up_pos = 0;
RAM static uint8_t  up_sec = 0;         /* first sector of the extent */
RAM static uint8_t  up_nsec = 0;
RAM static int16_t  up_erased = 0;      /* highest erased sector index inside the extent */
RAM static uint8_t  page_buf[256] = {0};

static void up_abort(void)
{
    if (up_active)
    {
        /* the header sector is erased, so nothing half written can be taken for a slide */
        fl_erase(SL_BASE + (uint32_t)up_sec * SL_SECSZ);
    }
    up_active = 0;
    sl_scan();
}

/* flush the page that contains extent offset `off` (page aligned) */
static int up_flush_page(uint32_t off)
{
    int16_t sec = (int16_t)(off / SL_SECSZ);
    while (up_erased < sec)
    {
        up_erased++;
        fl_erase(SL_BASE + ((uint32_t)up_sec + up_erased) * SL_SECSZ);
    }
    fl_write(SL_BASE + (uint32_t)up_sec * SL_SECSZ + off, page_buf, 256);
    memset(page_buf, 0xFF, sizeof(page_buf));
    return 0;
}

/* store n bytes at the upload position (the caller has checked that they fit) */
static void up_append(const uint8_t *d, unsigned int n)
{
    unsigned int i;
    for (i = 0; i < n; i++)
    {
        uint32_t off = SL_HDR + up_pos;
        page_buf[off & 255] = d[i];
        up_pos++;
        if ((off & 255) == 255)
            up_flush_page(off & ~255u);
    }
}

/* ---------------------------------------------------------------------------------------------
 * stored-image reader and decoder
 * ------------------------------------------------------------------------------------------- */
static uint32_t rd_addr;
static uint16_t rd_left;   /* bytes not yet fetched from the flash */
static uint16_t rd_in;     /* bytes not yet consumed */
static uint16_t rd_pos, rd_cnt;
static uint16_t rd_crc;
static uint8_t  rd_buf[256];

static void rd_start(uint32_t addr, uint16_t len)
{
    rd_addr = addr;
    rd_left = len;
    rd_in = len;
    rd_pos = rd_cnt = 0;
    rd_crc = 0xFFFF;
}

static int rd_byte(void)
{
    if (rd_in == 0)
        return -1;
    if (rd_pos == rd_cnt)
    {
        unsigned int n = rd_left > 256 ? 256 : rd_left;
        fl_read(rd_addr, n, rd_buf);
        rd_crc = crc16_update(rd_crc, rd_buf, n);
        rd_addr += n;
        rd_left -= n;
        rd_cnt = (uint16_t)n;
        rd_pos = 0;
    }
    rd_in--;
    return rd_buf[rd_pos++];
}

static int lz4_decode(uint8_t *dst, unsigned int dlen)
{
    unsigned int o = 0, lit, ml, off;
    int t, b;

    while (rd_in)
    {
        t = rd_byte();
        lit = (unsigned int)(t >> 4);
        if (lit == 15)
        {
            do
            {
                b = rd_byte();
                if (b < 0)
                    return -1;
                lit += (unsigned int)b;
            } while (b == 255);
        }
        if (o + lit > dlen)
            return -1;
        while (lit--)
        {
            b = rd_byte();
            if (b < 0)
                return -1;
            dst[o++] = (uint8_t)b;
        }
        if (rd_in == 0)
            break; /* the last sequence has literals only */
        b = rd_byte();
        t = (t & 15);
        if (b < 0 || rd_in == 0)
            return -1;
        off = (unsigned int)b;
        b = rd_byte();
        if (b < 0)
            return -1;
        off |= (unsigned int)b << 8;
        ml = (unsigned int)t;
        if (ml == 15)
        {
            do
            {
                b = rd_byte();
                if (b < 0)
                    return -1;
                ml += (unsigned int)b;
            } while (b == 255);
        }
        ml += 4;
        if (off == 0 || off > o || o + ml > dlen)
            return -1;
        while (ml--)
        {
            dst[o] = dst[o - off];
            o++;
        }
    }
    return o == dlen ? 0 : -1;
}

/* 1 bit per pixel (bit set = pal1) -> 2 bits per pixel in slide_img. src may be the upper half of slide_img itself:
   the output never overtakes the input. */
static void expand_1bpp(const uint8_t *src, uint8_t pal0, uint8_t pal1)
{
    unsigned int i;
    for (i = 0; i < IMG_BYTES / 2; i++)
    {
        uint8_t s = src[i];
        uint8_t o0 = 0, o1 = 0, k;
        for (k = 0; k < 4; k++)
        {
            o0 = (uint8_t)((o0 << 2) | ((s & (0x80 >> k)) ? pal1 : pal0));
            o1 = (uint8_t)((o1 << 2) | ((s & (0x08 >> k)) ? pal1 : pal0));
        }
        slide_img[2 * i] = o0;
        slide_img[2 * i + 1] = o1;
    }
}

/* decode slot into slide_img; returns 0 on success */
static int slide_load(uint8_t slot)
{
    uint8_t h[16];
    uint32_t base;
    unsigned int len, i, dlen;
    uint8_t flags, pal0, pal1;
    uint8_t *dst;
    int rc;

    if (slot >= SL_MAX_SLOTS || slot_start[slot] == NO_SLOT)
        return -1;
    base = SL_BASE + (uint32_t)slot_start[slot] * SL_SECSZ;
    fl_read(base, 16, h);
    if (!hdr_valid(h))
        return -1;
    len = (unsigned int)(h[4] | (h[5] << 8));
    flags = h[3];
    pal0 = h[8] & 3;
    pal1 = h[9] & 3;

    dlen = (flags & F_1BPP) ? IMG_BYTES / 2 : IMG_BYTES;
    dst = (flags & F_1BPP) ? slide_img + IMG_BYTES / 2 : slide_img; /* 1 bpp: decode into the upper half, expand in place */

    rd_start(base + SL_HDR, (uint16_t)len);
    if (flags & F_LZ4)
    {
        rc = lz4_decode(dst, dlen);
    }
    else
    {
        rc = (len == dlen) ? 0 : -1;
        for (i = 0; rc == 0 && i < dlen; i++)
        {
            int b = rd_byte();
            if (b < 0)
                rc = -1;
            else
                dst[i] = (uint8_t)b;
        }
    }
    while (rd_in)
        (void)rd_byte(); /* read the rest so the CRC covers the whole payload */
    if (rc != 0 || rd_crc != (uint16_t)(h[6] | (h[7] << 8)))
        return -1;

    if (flags & F_1BPP)
        expand_1bpp(slide_img + IMG_BYTES / 2, pal0, pal1);
    return 0;
}

/* ---------------------------------------------------------------------------------------------
 * showing slides
 * ------------------------------------------------------------------------------------------- */
RAM static uint8_t  cur_slot = NO_SLOT;
RAM static uint8_t  show_pending = NO_SLOT;
RAM static uint8_t  splash_up = 0;     /* the splash is on the screen */
RAM static uint8_t  was_conn = 0, leave_splash = 0;
/* Low battery: below BAT_LOW_MV the flash writes and the panel refreshes are stopped (a brown-out in the middle of a write or a refresh
 * would damage a slide or leave the panel half drawn).  The last picture stays on the e-paper without power.
 * The state is confirmed by two readings 30 s apart (battery_mv is refreshed every 30 s by app.c), and leaves only above BAT_OK_MV. */
#define BAT_LOW_MV   2400u
#define BAT_OK_MV    2500u
RAM static uint8_t  batt_low = 0, low_shown = 0, low_cnt = 0;
RAM static uint32_t batt_t = 0;
RAM static uint8_t  info_pending = 0; /* draw the hardware report on the tag screen */
RAM static uint32_t last_change = 0;
RAM static uint8_t  boot_done = 0;
RAM static uint8_t  last_cmd = 0, last_res = 0;
RAM static uint32_t rnd_state = 12345;

static uint8_t next_slot(int dir)
{
    unsigned int i;
    if (pl_count == 0)
        return NO_SLOT;
    if (cfg.order && pl_count > 1 && dir == 0)
    {
        uint8_t pick;
        rnd_state = rnd_state * 1664525u + 1013904223u + clock_time();
        do
        {
            pick = playlist[(rnd_state >> 8) % pl_count];
            rnd_state = rnd_state * 1664525u + 1013904223u;
        } while (pick == cur_slot);
        return pick;
    }
    if (cur_slot == NO_SLOT)
        return playlist[dir ? pl_count - 1 : 0];
    for (i = 0; i < pl_count; i++)
        if (playlist[i] >= cur_slot)
            break;
    /* i = index of the first slide with a number >= current */
    if (dir == 0)
    {
        if (i < pl_count && playlist[i] == cur_slot)
            i++;
        return playlist[i >= pl_count ? 0 : i];
    }
    return playlist[i == 0 ? pl_count - 1 : i - 1];
}

/* start-up screen, fixed in the firmware (tools/make_splash.py; bit set = black) */
static void do_show_splash(void)
{
    expand_1bpp(splash_1bpp, 1, 0);
    cur_slot = NO_SLOT;
    splash_up = 1;
    last_change = get_time();
    EPD_BWRY_154_show(slide_img);
}

static void do_show_low(void)
{
    char ln[TXT_LINES][TXT_COLS + 1];
    hwinfo_low_text(ln, battery_mv);
    textscr_draw(slide_img, "LOW BATTERY", ln, 4);
    cur_slot = NO_SLOT;
    splash_up = 0;
    low_shown = 1;
    last_change = get_time();
    EPD_BWRY_154_show(slide_img);
}

int slides_low_battery(void)
{
    return batt_low;
}

/* called from the tick; direct = measure now (start-up, nobody transmits) instead of waiting for the periodic reading */
static void batt_update(uint8_t direct)
{
    uint16_t mv;
    if (direct)
    {
        battery_mv = get_battery_mv();
        mv = battery_mv;
        low_cnt = (mv && mv < BAT_LOW_MV) ? 2 : 0; /* a single reading is enough at the start */
        batt_low = low_cnt >= 2;
        batt_t = get_time();
        return;
    }
    if ((uint32_t)(get_time() - batt_t) < 30)
        return;
    batt_t = get_time();
    mv = battery_mv;
    if (mv && mv < BAT_LOW_MV)
    {
        if (low_cnt < 2)
            low_cnt++;
    }
    else if (mv >= BAT_OK_MV)
        low_cnt = 0;
    if (low_cnt >= 2)
        batt_low = 1;
    else if (low_cnt == 0)
        batt_low = 0;
}

static int do_show(uint8_t slot)
{
    splash_up = 0;
    if (slide_load(slot) != 0)
        return -1;
    cur_slot = slot;
    last_change = get_time();
    EPD_BWRY_154_show(slide_img);
    return 0;
}

/* ---------------------------------------------------------------------------------------------
 * status for the read characteristic
 * ------------------------------------------------------------------------------------------- */
static void resp_update(void)
{
    uint8_t *r = my_EPD_BLE_Data;
    extern uint8_t epd_update_state;
    unsigned int fs = free_sectors();
    uint16_t mv = battery_mv;

    r[0] = last_cmd;
    r[1] = last_res;
    r[2] = (uint8_t)((up_active ? 1 : 0) | (epd_update_state ? 2 : 0) | ((show_pending != NO_SLOT || info_pending) ? 4 : 0) | (batt_low ? 8 : 0));
    r[3] = pl_count;
    r[4] = cur_slot;
    r[5] = (uint8_t)fs;
    r[6] = cfg.interval_min & 0xFF;
    r[7] = cfg.interval_min >> 8;
    r[8] = cfg.order;
    r[9] = cfg.flags;
    r[10] = mv & 0xFF;
    r[11] = mv >> 8;
    r[12] = SL_FW_VERSION;
    r[13] = up_pos & 0xFF;
    r[14] = up_pos >> 8;
    r[15] = (uint8_t)SL_SECTORS;
    r[16] = SL_WRITE_MAX;
    r[17] = 0;
    r[18] = 0;
    r[19] = 0x5A;
}

/* seconds to the next automatic change, 0xFFFFFFFF = none planned (the change also waits for the page to disconnect) */
static uint32_t next_change_s(void)
{
    uint32_t total, el;
    if (!cfg.interval_min || up_active || !(pl_count > 1 || (pl_count == 1 && cur_slot == NO_SLOT)))
        return 0xFFFFFFFFu;
    total = (uint32_t)cfg.interval_min * 60u;
    el = (uint32_t)(get_time() - last_change);
    return el >= total ? 0u : total - el;
}

static void hw_slides(hw_slides_t *hp)
{
    hw_slides_t h;
    h.next_s = next_change_s();
    h.sectors = (uint8_t)SL_SECTORS;
    h.free_sectors = (uint8_t)free_sectors();
    h.count = pl_count;
    h.interval_min = cfg.interval_min;
    *hp = h;
}

static void resp_name(void)
{
    uint8_t *r = my_EPD_BLE_Data;
    unsigned int i;
    for (i = 0; i < 20; i++)
        r[i] = 0;
    r[0] = 0x31;
    r[1] = R_OK;
    r[2] = cfg.name_len;
    memcpy(r + 3, cfg.name, cfg.name_len);
    r[19] = 0x5A;
}

static void resp_hw(uint8_t page)
{
    hw_slides_t h;
    hw_slides(&h);
    hwinfo_fill(page, my_EPD_BLE_Data, &h);
}

static void resp_list(void)
{
    uint8_t *r = my_EPD_BLE_Data;
    unsigned int i;
    r[0] = 0x29;
    r[1] = R_OK;
    for (i = 2; i < 20; i++)
        r[i] = 0;
    for (i = 0; i < SL_MAX_SLOTS; i++)
        if (slot_start[i] != NO_SLOT)
            r[2 + (i >> 3)] |= (uint8_t)(1 << (i & 7)); /* bytes 2..16: bitmap of the slots in use */
    r[19] = 0x5A;                                       /* every answer ends with 0x5A, the page checks it */
}

RAM static uint8_t list_mode = 0;

/* ---------------------------------------------------------------------------------------------
 * BLE commands
 * ------------------------------------------------------------------------------------------- */
int slides_ble_cmd(const uint8_t *p, unsigned int len)
{
    uint8_t cmd = p[0];
    uint8_t res = R_OK;
    unsigned int i;

    list_mode = 0;
    if (batt_low && (cmd == 0x20 || cmd == 0x21 || cmd == 0x22 || (cmd >= 0x23 && cmd <= 0x27) || cmd == 0x2E || cmd == 0x2F || cmd == 0x30 || cmd == 0x32))
    {
        if (up_active)
            up_abort(); /* the battery fell in the middle of an upload: stop writing the flash */
        last_cmd = cmd;
        last_res = R_LOWBAT;
        return 0;
    }
    switch (cmd)
    {
    case 0x20: /* BEGIN: slot, flags, len(2), crc(2), pal0, pal1 */
    {
        unsigned int l, need;
        int at;
        if (len < 9)
        {
            res = R_BADARG;
            break;
        }
        if (up_active)
            up_abort();
        l = (unsigned int)(p[3] | (p[4] << 8));
        if (p[1] >= SL_MAX_SLOTS || l == 0 || l > SL_MAX_LEN || (p[2] & ~(F_LZ4 | F_1BPP)))
        {
            res = R_BADARG;
            break;
        }
        if (battery_mv && battery_mv < 2300)
        {
            res = R_LOWBAT; /* flash writes are not reliable at this voltage */
            break;
        }
        slot_delete(p[1]);
        need = sectors_for(l);
        at = find_run(need);
        if (at < 0)
        {
            res = R_NOSPACE;
            break;
        }
        up_slot = p[1];
        up_flags = p[2];
        up_len = (uint16_t)l;
        up_crc = (uint16_t)(p[5] | (p[6] << 8));
        up_pal0 = p[7] & 3;
        up_pal1 = p[8] & 3;
        up_sec = (uint8_t)at;
        up_nsec = (uint8_t)need;
        up_pos = 0;
        memset(page_buf, 0xFF, sizeof(page_buf));
        fl_erase(SL_BASE + (uint32_t)up_sec * SL_SECSZ);
        up_erased = 0;
        up_active = 1;
        ble_set_connection_speed(6); /* 7.5 .. 10 ms during an upload (the phone decides); back to 250 ms afterwards */
        break;
    }
    case 0x21: /* DATA: bytes */
    {
        unsigned int n = len - 1;
        if (!up_active)
        {
            res = R_NOUPLOAD;
            break;
        }
        if (up_pos + n > up_len)
        {
            up_abort();
            res = R_BADARG;
            break;
        }
        up_append(p + 1, n);
        break;
    }
    case 0x30: /* DATA AT OFFSET: offset(2), bytes.  Meant for writes without response in a window: a chunk that is
                  not the next one in line is not stored (a repeat is accepted silently); the page reads the position
                  from the status and continues from there. */
    {
        unsigned int n, off;
        if (len < 4)
        {
            res = R_BADARG;
            break;
        }
        if (!up_active)
        {
            res = R_NOUPLOAD;
            break;
        }
        n = len - 3;
        off = (unsigned int)(p[1] | (p[2] << 8));
        if (off + n <= up_pos)
            break; /* already stored */
        if (off != up_pos)
        {
            res = R_BADARG; /* a gap (or a partial overlap): wait for the page to resend from the status position */
            break;
        }
        if (up_pos + n > up_len)
        {
            up_abort();
            res = R_BADARG;
            break;
        }
        up_append(p + 3, n);
        break;
    }
    case 0x31: /* NAME: read */
        resp_name();
        list_mode = 3; /* the answer stays in the status buffer */
        break;
    case 0x32: /* NAME: set (no bytes = the default name) */
    {
        unsigned int n = len - 1;
        if (n > TAGNAME_MAX)
        {
            res = R_BADARG;
            break;
        }
        for (i = 0; i < n; i++)
            if (p[1 + i] < 0x20 || p[1 + i] == 0x7F)
                res = R_BADARG; /* control characters */
        if (res != R_OK)
            break;
        cfg.name_len = (uint8_t)n;
        memset(cfg.name, 0, sizeof(cfg.name));
        memcpy(cfg.name, p + 1, n);
        cfg_save();
        tagname_apply(cfg.name, cfg.name_len);
        break;
    }
    case 0x22: /* END */
    {
        uint8_t h[16], chk[16];
        uint32_t base;
        uint16_t crc = 0xFFFF;
        uint16_t left;
        uint32_t addr;
        if (!up_active)
        {
            res = R_NOUPLOAD;
            break;
        }
        if (up_pos != up_len)
        {
            up_abort();
            res = R_BADARG;
            break;
        }
        {
            uint32_t off = SL_HDR + up_pos; /* offset after the last byte */
            if (off & 255)
                up_flush_page(off & ~255u);
        }
        base = SL_BASE + (uint32_t)up_sec * SL_SECSZ;
        addr = base + SL_HDR;
        left = up_len;
        while (left)
        {
            uint8_t buf[128];
            unsigned int n = left > sizeof(buf) ? sizeof(buf) : left;
            fl_read(addr, n, buf);
            crc = crc16_update(crc, buf, n);
            addr += n;
            left -= (uint16_t)n;
        }
        if (crc != up_crc)
        {
            up_abort();
            res = R_CRC;
            break;
        }
        memset(h, 0xFF, sizeof(h));
        h[0] = SL_MAGIC & 0xFF;
        h[1] = SL_MAGIC >> 8;
        h[2] = up_slot;
        h[3] = up_flags;
        h[4] = up_len & 0xFF;
        h[5] = up_len >> 8;
        h[6] = up_crc & 0xFF;
        h[7] = up_crc >> 8;
        h[8] = up_pal0;
        h[9] = up_pal1;
        h[10] = hdr_chk(h);
        fl_write(base, h, 16); /* the header goes last: only now the slide exists */
        fl_read(base, 16, chk);
        up_active = 0;
        if (memcmp(h, chk, 16) != 0)
        {
            fl_erase(base);
            sl_scan();
            res = R_FLASH;
            break;
        }
        sl_scan();
        ble_set_connection_speed(200);
        break;
    }
    case 0x23: /* DELETE: slot */
        if (len < 2 || p[1] >= SL_MAX_SLOTS)
        {
            res = R_BADARG;
            break;
        }
        if (slot_start[p[1]] == NO_SLOT)
        {
            res = R_NOTFOUND;
            break;
        }
        slot_delete(p[1]);
        if (cur_slot == p[1])
            cur_slot = NO_SLOT;
        break;
    case 0x24: /* CLEAR all slides (headers only: fast) */
        if (up_active)
            up_abort();
        for (i = 0; i < SL_MAX_SLOTS; i++)
            if (slot_start[i] != NO_SLOT)
                slot_delete((uint8_t)i);
        cur_slot = NO_SLOT;
        break;
    case 0x25: /* SHOW: slot */
        if (len < 2 || p[1] >= SL_MAX_SLOTS)
        {
            res = R_BADARG;
            break;
        }
        if (slot_start[p[1]] == NO_SLOT)
        {
            res = R_NOTFOUND;
            break;
        }
        show_pending = p[1];
        break;
    case 0x26: /* CONFIG: interval(2), order, flags */
        if (len < 5)
        {
            res = R_BADARG;
            break;
        }
        cfg.interval_min = (uint16_t)(p[1] | (p[2] << 8));
        cfg.order = p[3] & 1;
        cfg.flags = p[4];
        cfg_save();
        last_change = get_time();
        break;
    case 0x27: /* STEP: 0 = next, 1 = previous */
    {
        uint8_t s;
        if (len < 2)
        {
            res = R_BADARG;
            break;
        }
        s = next_slot(p[1] ? 1 : 0);
        if (s == NO_SLOT)
            res = R_NOTFOUND;
        else
            show_pending = s;
        break;
    }
    case 0x28: /* INFO */
        break;
    case 0x29: /* LIST */
        list_mode = 1;
        break;
    case 0x2B: /* NFC probe: sub, arg */
        nfc_probe_run(len > 1 ? p[1] : 0, len > 2 ? p[2] : 0);
        list_mode = 2;
        break;
    case 0x2C: /* hardware report: page */
        resp_hw(len > 1 ? p[1] : 0);
        list_mode = 3; /* the answer stays in the status buffer */
        break;
    case 0x2D: /* watchdog test: hang on purpose, the watchdog must restart the tag within WDT_MS */
        if (len > 1 && p[1] == 0xA5)
            wdt_test_request();
        else
            res = R_BADARG;
        break;
    case 0x2F: /* TX power: level index */
        if (len < 2 || p[1] >= TX_LEVELS)
        {
            res = R_BADARG;
            break;
        }
        cfg.tx = p[1];
        cfg_save();
        txpower_set(cfg.tx);
        break;
    case 0x2E: /* hardware report on the tag screen */
        if (up_active)
            res = R_BUSY;
        else
            info_pending = 1;
        break;
    case 0x2A: /* ABORT upload */
        up_abort();
        ble_set_connection_speed(200);
        break;
    default:
        res = R_UNKNOWN;
        break;
    }

    last_cmd = cmd;
    last_res = res;
    return 0;
}

/* ---------------------------------------------------------------------------------------------
 * init and periodic work
 * ------------------------------------------------------------------------------------------- */
void slides_init(void)
{
    cfg_load();
    txpower_set(cfg.tx);
    tagname_apply(cfg.name, cfg.name_len);
    sl_scan();
    last_change = get_time();
}

void slides_tick(void)
{
    extern uint8_t epd_update_state;
    uint8_t tap;

    nfc_poll();
    led_link_tick();
    if (boot_done)
        batt_update(0);
    tap = nfc_tap_take(); /* a tap during a panel refresh is dropped */
    {
        uint8_t c = ble_get_connected() ? 1 : 0;
        if (was_conn && !c && splash_up)
            leave_splash = 1; /* the phone has disconnected while the splash is shown: the slides begin */
        was_conn = c;
    }
    if (list_mode == 1)
        resp_list();
    else if (list_mode == 2)
        nfc_probe_fill(my_EPD_BLE_Data);
    else if (list_mode == 3)
        ; /* hardware report: written by the command */
    else
        resp_update();

    if (epd_update_state)
        return; /* the panel is refreshing */

    if (batt_low)
    {
        show_pending = NO_SLOT;
        info_pending = 0;
        if (!low_shown && boot_done)
            do_show_low(); /* once: the warning stays on the panel, nothing else is refreshed */
        return;
    }
    if (low_shown)
    {
        uint8_t s = next_slot(0); /* the battery is fine again: back to the slides */
        low_shown = 0;
        if (s != NO_SLOT)
            do_show(s);
        return;
    }

    if (info_pending)
    {
        char ln[TXT_LINES][TXT_COLS + 1];
        hw_slides_t h;
        info_pending = 0;
        hw_slides(&h);
        hwinfo_text(ln, &h);
        textscr_draw(slide_img, "TAG HARDWARE", ln, TXT_LINES);
        cur_slot = NO_SLOT; /* like the splash: the slides continue after the interval */
        splash_up = 0;
        last_change = get_time();
        EPD_BWRY_154_show(slide_img);
        return;
    }

    if (show_pending != NO_SLOT)
    {
        uint8_t s = show_pending;
        show_pending = NO_SLOT;
        if (do_show(s) != 0)
        {
            last_res = R_FLASH;
            sl_scan();
        }
        return;
    }

    if (!boot_done)
    {
        if (get_time() >= 3)
        {
            boot_done = 1;
            batt_update(1);
            set_led_color(3); /* one blue blink: the start-up splash begins */
            WaitMs(200);
            set_led_color(0);
            WaitMs(200);
            if (batt_low)
                do_show_low(); /* a nearly empty battery: the warning instead of the splash */
            else
                do_show_splash(); /* every start (new battery, reset) begins with the splash; the slides follow after the interval */
        }
        return;
    }

    if (leave_splash && !up_active && !ble_get_connected())
    {
        uint8_t s;
        leave_splash = 0;
        s = splash_up ? next_slot(0) : NO_SLOT;
        if (s != NO_SLOT)
            do_show(s);
        return;
    }

    if (tap && !(cfg.flags & 1) && !up_active && !ble_get_connected())
    {
        uint8_t s = next_slot(0); /* NFC tap = next slide */
        if (s != NO_SLOT)
        {
            set_led_color(3); /* short blue blink: tap recognised */
            WaitMs(80);
            set_led_color(0);
            do_show(s);
        }
        return;
    }

    if (cfg.interval_min && (pl_count > 1 || (pl_count == 1 && cur_slot == NO_SLOT)) && !up_active && !ble_get_connected() &&
        (uint32_t)(get_time() - last_change) >= (uint32_t)cfg.interval_min * 60u)
    {
        uint8_t s = next_slot(0);
        if (s != NO_SLOT)
            do_show(s);
    }
}

#ifdef SLIDES_HOST
/* host tests only: act as after a power-up (the retention RAM is lost) */
void slides_host_reboot(void)
{
    boot_done = 0;
    splash_up = 0;
    was_conn = 0;
    leave_splash = 0;
    cur_slot = NO_SLOT;
}
#endif
