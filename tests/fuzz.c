/* Fuzz test of slides.c: random and hostile BLE commands, hostile slide payloads (with a correct CRC, so the decoder runs),
 * damaged flash contents.  Built with AddressSanitizer + UndefinedBehaviorSanitizer by run.sh; any memory error or undefined
 * behaviour aborts the run.  Deterministic: FUZZ_SEED and FUZZ_ITERS (environment) select the run.
 *
 * Invariants checked after every step:
 *   - the status buffer ends with 0x5A (the page relies on it)
 *   - the flash is written only inside the slide area and the settings sector, and only by legal program/erase operations */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t flash_mem[];
extern uint8_t my_EPD_BLE_Data[20];
extern uint8_t epd_update_state;
extern uint16_t battery_mv;
extern uint32_t fake_time;
extern int fake_conn, fake_tap, write_violations, region_violations, show_count;
void host_reset(void);
void slides_init(void);
void slides_tick(void);
int  slides_ble_cmd(const uint8_t *payload, unsigned int len);
void slides_host_reboot(void);

static uint64_t rs = 88172645463325252ULL;
static uint32_t rnd(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 7;
    rs ^= rs << 17;
    return (uint32_t)(rs >> 16);
}
static uint32_t rn(uint32_t n) { return n ? rnd() % n : 0; }

static uint16_t crc16(const uint8_t *d, unsigned int n)
{
    uint16_t crc = 0xFFFF;
    unsigned int i;
    for (i = 0; i < n; i++)
    {
        int b;
        crc ^= (uint16_t)d[i] << 8;
        for (b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

static unsigned long steps = 0;

static void check(const char *what)
{
    steps++;
    if (my_EPD_BLE_Data[19] != 0x5A)
    {
        fprintf(stderr, "FUZZ FAIL: status buffer without the end marker after %s (step %lu)\n", what, steps);
        exit(1);
    }
    if (write_violations || region_violations)
    {
        fprintf(stderr, "FUZZ FAIL: flash misuse after %s (step %lu): violations %d, outside the allowed area %d\n", what, steps,
                write_violations, region_violations);
        exit(1);
    }
}

static void send(const uint8_t *p, unsigned int len)
{
    slides_ble_cmd(p, len);
    slides_tick();
    check("command");
}

static void tick_time(uint32_t s)
{
    fake_time += s;
    slides_tick();
    check("tick");
}

/* random bytes, or the likeness of an LZ4 stream: tokens, literals, offsets (also 0 and too large), long runs */
static unsigned int make_payload(uint8_t *b, unsigned int max, int lz4like)
{
    unsigned int n = 0, i;
    if (lz4like && rn(6) == 0)
    {
        /* one huge literal run (longer than the 5000 bytes of a 1 bpp image) */
        unsigned int L = 4900 + rn(5000), r = L - 15;
        b[n++] = 0xF0;
        while (r >= 255) { b[n++] = 255; r -= 255; }
        b[n++] = (uint8_t)r;
        for (i = 0; i < L; i++)
            b[n++] = (uint8_t)rnd();
        return n;
    }
    if (!lz4like)
    {
        n = 1 + rn(max);
        for (i = 0; i < n; i++)
            b[i] = (uint8_t)rnd();
        return n;
    }
    while (n + 8 < max)
    {
        unsigned int lit = rn(4) == 0 ? 15 + rn(300) : rn(16), mat = rn(4) == 0 ? 15 + rn(600) : rn(16);
        uint8_t tok = (uint8_t)(((lit > 15 ? 15 : lit) << 4) | (mat > 15 ? 15 : mat));
        b[n++] = tok;
        if (lit >= 15)
        {
            unsigned int r = lit - 15;
            while (r >= 255 && n + 4 < max) { b[n++] = 255; r -= 255; }
            b[n++] = (uint8_t)r;
        }
        for (i = 0; i < lit && n + 4 < max; i++)
            b[n++] = (uint8_t)rnd();
        if (rn(12) == 0)
            break; /* a final sequence without a match */
        b[n++] = (uint8_t)(rn(3) == 0 ? rnd() : 1 + rn(40)); /* offset, low byte */
        b[n++] = (uint8_t)(rn(5) == 0 ? rnd() : 0);          /* high byte */
        if (mat >= 15)
        {
            unsigned int r = mat - 15;
            while (r >= 255 && n + 2 < max) { b[n++] = 255; r -= 255; }
            b[n++] = (uint8_t)r;
        }
        if (rn(60) == 0)
            break;
    }
    if (n == 0)
        b[n++] = 0;
    return n;
}

/* a complete upload with a correct CRC and chunks of random size; optionally a show afterwards */
static void upload_payload(uint8_t slot, uint8_t flags, const uint8_t *pl, unsigned int len, unsigned int chunk)
{
    uint8_t cmd[64];
    unsigned int i;
    uint16_t c = crc16(pl, len);
    int by_offset = rn(3) == 0;
    cmd[0] = 0x20; cmd[1] = slot; cmd[2] = flags;
    cmd[3] = (uint8_t)len; cmd[4] = (uint8_t)(len >> 8);
    cmd[5] = (uint8_t)c; cmd[6] = (uint8_t)(c >> 8);
    cmd[7] = (uint8_t)rn(4); cmd[8] = (uint8_t)rn(4);
    send(cmd, 9);
    for (i = 0; i < len; i += chunk)
    {
        unsigned int n = len - i < chunk ? len - i : chunk;
        if (by_offset)
        {
            cmd[0] = 0x30; /* data at an offset, sometimes repeated */
            cmd[1] = (uint8_t)i; cmd[2] = (uint8_t)(i >> 8);
            memcpy(cmd + 3, pl + i, n);
            send(cmd, n + 3);
            if (rn(8) == 0)
                send(cmd, n + 3);
        }
        else
        {
            cmd[0] = 0x21;
            memcpy(cmd + 1, pl + i, n);
            send(cmd, n + 1);
        }
        if (rn(200) == 0)
            tick_time(rn(40)); /* time passes during the upload */
    }
    cmd[0] = 0x22;
    send(cmd, 1);
}

/* a correct LZ4 stream for dlen bytes of output (literals and matches with valid offsets), so that the decoder reaches its success path */
static unsigned int valid_lz4(uint8_t *b, unsigned int dlen)
{
    unsigned int o = 0, n = 0, i;
    while (o < dlen)
    {
        unsigned int rem = dlen - o, lit = (o == 0 || rn(3) == 0) ? 1 + rn(24) : rn(4), mlen = 4 + (rn(5) == 0 ? rn(600) : rn(40)), off;
        if (lit > rem)
            lit = rem;
        if (rem - lit < 4)
            lit = rem; /* the rest as literals: the last sequence has no match */
        {
            unsigned int mm = (rem - lit >= 4 && lit != rem) ? mlen : 0;
            if (mm > rem - lit)
                mm = rem - lit;
            if (mm < 4 && lit != rem)
            {
                lit = rem;
                mm = 0;
            }
            b[n] = (uint8_t)(((lit > 15 ? 15 : lit) << 4) | (mm ? (mm - 4 > 15 ? 15 : mm - 4) : 0));
            n++;
            if (lit >= 15)
            {
                unsigned int r = lit - 15;
                while (r >= 255) { b[n++] = 255; r -= 255; }
                b[n++] = (uint8_t)r;
            }
            for (i = 0; i < lit; i++)
                b[n++] = (uint8_t)rnd();
            o += lit;
            if (!mm)
                break;
            off = 1 + rn(o < 65535 ? o : 65535);
            b[n++] = (uint8_t)off;
            b[n++] = (uint8_t)(off >> 8);
            if (mm - 4 >= 15)
            {
                unsigned int r = mm - 4 - 15;
                while (r >= 255) { b[n++] = 255; r -= 255; }
                b[n++] = (uint8_t)r;
            }
            o += mm;
        }
    }
    return n;
}

/* valid slides (raw and LZ4, 2 and 1 bit per pixel), sometimes damaged; then show, step, delete, timer */
static void op_valid_slide(void)
{
    uint8_t pl[10600], cmd[8];
    uint8_t flags = (uint8_t)rn(4);
    unsigned int dlen = (flags & 2) ? 5000 : 10000, len, k;
    uint8_t slot = (uint8_t)rn(12);
    if (flags & 1)
        len = valid_lz4(pl, dlen);
    else
    {
        len = dlen;
        for (k = 0; k < len; k++)
            pl[k] = (uint8_t)rnd();
    }
    if (rn(4) == 0 && len)
        pl[rn(len)] ^= (uint8_t)(1 + rn(255)); /* one damaged byte, the CRC is recomputed */
    if (rn(8) == 0 && len > 2)
        len -= 1 + rn(len / 2); /* cut short */
    if (len > 10000)
        len = 10000;
    upload_payload(slot, flags, pl, len, 1 + rn(19));
    cmd[0] = 0x26; cmd[1] = (uint8_t)rn(3); cmd[2] = 0; cmd[3] = (uint8_t)rn(2); cmd[4] = (uint8_t)rn(2);
    send(cmd, 5);
    cmd[0] = 0x25; cmd[1] = slot;
    send(cmd, 2);
    for (k = 0; k < 4; k++)
    {
        tick_time(rn(3) == 0 ? 61 : 1);
        epd_update_state = 0;
        if (rn(3) == 0) { cmd[0] = 0x27; cmd[1] = (uint8_t)rn(2); send(cmd, 2); }
        if (rn(6) == 0) { cmd[0] = 0x23; cmd[1] = (uint8_t)rn(12); send(cmd, 2); }
    }
}

static void op_random_command(void)
{
    uint8_t b[260];
    unsigned int len = rn(8) == 0 ? rn(255) : rn(24), i;
    for (i = 0; i < len; i++)
        b[i] = (uint8_t)rnd();
    if (len)
        b[0] = (uint8_t)(rn(5) == 0 ? rnd() : 0x10 + rn(0x30)); /* mostly real command numbers */
    if (len && b[0] == 0x2D)
        b[0] = 0x2F; /* 0x2D hangs the tag on purpose (watchdog test) when the magic byte is right */
    send(b, len);
}

static void op_hostile_slide(void)
{
    uint8_t pl[10100];
    int lz4like = rn(3) != 0;
    uint8_t flags = (uint8_t)rn(4);
    unsigned int max = rn(6) == 0 ? 10000 : 20 + rn(3000), len = make_payload(pl, max, lz4like);
    uint8_t slot = (uint8_t)rn(12);
    upload_payload(slot, flags, pl, len, 1 + rn(19));
    {
        uint8_t s[2] = {0x25, slot};
        send(s, 2); /* shows it: the decoder runs on the hostile data */
        tick_time(1);
        epd_update_state = 0;
        tick_time(1);
    }
}

static void op_damage_flash(void)
{
    unsigned int k, n = 1 + rn(40);
    for (k = 0; k < n; k++)
    {
        uint32_t a = 0x14000u + rn(0x62000u);
        if (rn(4) == 0)
            a = 0x79000u + rn(32); /* the settings */
        if (rn(3) == 0)
            a = 0x14000u + 4096u * rn(98) + rn(24); /* the header of a slide record */
        flash_mem[a] = (uint8_t)rnd();
    }
    slides_init();
    slides_tick(); /* the main loop ticks right after the init: the status buffer is filled by the tick */
    check("init on a damaged flash");
    {
        uint8_t c[2] = {0x29, 0};
        send(c, 1);
        c[0] = 0x28;
        send(c, 1);
        c[0] = 0x25; c[1] = (uint8_t)rn(120);
        send(c, 2);
        tick_time(1);
        epd_update_state = 0;
        tick_time(61 * 60);
    }
}

static void op_environment(void)
{
    switch (rn(8))
    {
    case 0: fake_conn = (int)rn(2); break;
    case 1: fake_tap = 1; break;
    case 2: epd_update_state = (uint8_t)rn(2); break;
    case 3: battery_mv = (uint16_t)(rn(3) == 0 ? 2200 + rn(400) : 3000); break;
    case 4: fake_time += rn(3) == 0 ? rn(7 * 3600) : rn(90); break;
    case 5: slides_host_reboot(); break;
    case 6: if (rn(30) == 0) { host_reset(); slides_init(); } break;
    default: break;
    }
    slides_tick();
    check("environment");
}

int main(void)
{
    const char *e;
    unsigned long iters = 3000, i;
    unsigned long seed = 1;
    if ((e = getenv("FUZZ_ITERS")))
        iters = strtoul(e, 0, 10);
    if ((e = getenv("FUZZ_SEED")))
        seed = strtoul(e, 0, 10);
    rs ^= (uint64_t)seed * 0x9E3779B97F4A7C15ULL;
    host_reset();
    slides_init();
    slides_host_reboot();
    fake_time = 100;
    for (i = 0; i < iters; i++)
    {
        switch (rn(10))
        {
        case 0: case 1: case 2: case 3: op_random_command(); break;
        case 4: op_hostile_slide(); break;
        case 5: op_valid_slide(); break;
        case 6: op_damage_flash(); break;
        default: op_environment(); break;
        }
        if (battery_mv < 2400 && rn(4) == 0)
            battery_mv = 3000; /* do not stay in the low-battery state for the whole run */
    }
    printf("fuzz: seed %lu, %lu operations, %lu steps, %d panel refreshes, no errors\n", seed, iters, steps, show_count);
    return 0;
}
