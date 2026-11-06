#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "main.h"
#include "i2c.h"
#include "nfc_probe.h"

#define NFC_ID 0xAE /* write address; the driver adds the read bit */

RAM static uint8_t np_buf[20];
RAM static uint8_t np_last = 2;  /* last sampled IRQ level, 2 = none yet */
RAM static uint8_t np_fall = 0;
RAM static uint8_t np_rise = 0;
RAM static uint8_t np_seen = 0;  /* bit0: pin was seen low, bit1: pin was seen high */
/* what the chip reported when the IRQ pin was seen low: index 0 MAIN_IRQ, 1 FIFO_IRQ, 2 AUX_IRQ, 3 RF_STATUS */
RAM static uint8_t ev_cnt = 0;
RAM static uint8_t ev_or[4] = {0, 0, 0, 0};
RAM static uint8_t ev_last[4] = {0, 0, 0, 0};
RAM static uint8_t ev_first[4] = {0, 0, 0, 0};

static void irq_pin_setup(void)
{
    gpio_set_func(NFC_IRQ, AS_GPIO);
    gpio_set_output_en(NFC_IRQ, 0);
    gpio_set_input_en(NFC_IRQ, 1);
    gpio_setup_up_down_resistor(NFC_IRQ, PM_PIN_PULLUP_10K);
}

static uint8_t nfc_rd(uint16_t reg)
{
    uint8_t v = 0xEE;
    i2c_set_id(NFC_ID);
    i2c_read_series(reg, 2, &v, 1);
    return v;
}

static void nfc_wr(uint16_t reg, uint8_t val)
{
    uint8_t b[3];
    b[0] = (uint8_t)(reg >> 8);
    b[1] = (uint8_t)reg;
    b[2] = val;
    send_i2c(NFC_ID, b, 3);
}

static void nfc_open(void)
{
    init_i2c(); /* the I2C block loses its settings in deep sleep */
    gpio_write(NFC_CS, 1);
    gpio_set_func(NFC_CS, AS_GPIO);
    gpio_set_output_en(NFC_CS, 1);
    gpio_set_input_en(NFC_CS, 0);
    gpio_write(NFC_CS, 0);
    sleep_us(500);
}

static void nfc_close(void)
{
    gpio_write(NFC_CS, 1);
}

void nfc_poll(void)
{
    uint8_t lv;
    irq_pin_setup();
    lv = gpio_read(NFC_IRQ) ? 1 : 0;
    np_seen |= (uint8_t)(1u << lv);
    if (np_last != 2 && lv != np_last)
    {
        if (lv)
        {
            if (np_rise < 255)
                np_rise++;
        }
        else if (np_fall < 255)
            np_fall++;
    }
    np_last = lv;
    if (lv == 0)
    {
        /* the pin is low: ask the chip why (reading the flags also releases the pin) */
        uint8_t v[4], k;
        nfc_open();
        v[0] = nfc_rd(0xFFF7);
        v[1] = nfc_rd(0xFFF8);
        v[2] = nfc_rd(0xFFF9);
        v[3] = nfc_rd(0xFFF3);
        nfc_close();
        if (ev_cnt == 0)
            for (k = 0; k < 4; k++)
                ev_first[k] = v[k];
        for (k = 0; k < 4; k++)
        {
            ev_or[k] |= v[k];
            ev_last[k] = v[k];
        }
        if (ev_cnt < 255)
            ev_cnt++;
    }
}

void nfc_probe_run(uint8_t sub, uint8_t arg)
{
    /* registers of page A / page B, 12 bytes each, stored from byte 7 */
    static const uint16_t pa[12] = {0xFFF3, 0xFFF7, 0xFFF8, 0xFFF9, 0xFFFA, 0xFFFB,
                                    0xFFFC, 0xFFFD, 0xFFE0, 0xFFE1, 0xFFE2, 0xFFE7};
    static const uint16_t pb[12] = {0x03B0, 0x03B1, 0x03B2, 0x03B3, 0x03B4, 0x03B5,
                                    0x03B6, 0x03B7, 0xFFF2, 0xFFF4, 0xFFF5, 0xFFF6};
    uint8_t i, ack;

    for (i = 0; i < 20; i++)
        np_buf[i] = 0;
    np_buf[0] = 0x2B;

    nfc_open();

    ack = test_i2c_device(NFC_ID >> 1);
    np_buf[2] = (uint8_t)(ack | (sub << 4));

    if (sub == 2) /* register setup like the OEPL firmware does (volatile registers only) */
    {
        nfc_wr(0xFFFB, 0x0F);
        nfc_wr(0xFFFC, 0x18);
        nfc_wr(0xFFFD, 0x03);
        nfc_wr(0xFFF1, 0xFF);
        nfc_wr(0xFFF4, 0x77);
    }
    else if (sub == 3) /* main IRQ mask */
        nfc_wr(0xFFFA, arg);
    else if (sub == 4) /* AUX IRQ mask */
        nfc_wr(0xFFFC, arg);
    else if (sub == 5) /* counters back to zero */
    {
        np_fall = np_rise = np_seen = 0;
        np_last = 2;
        ev_cnt = 0;
        for (i = 0; i < 4; i++)
            ev_or[i] = ev_last[i] = ev_first[i] = 0;
    }

    if (sub == 6) /* what was reported at the last IRQ events (no chip access needed) */
    {
        np_buf[7] = ev_cnt;
        for (i = 0; i < 4; i++)
        {
            np_buf[8 + i] = ev_or[i];
            np_buf[12 + i] = ev_last[i];
        }
        np_buf[16] = ev_first[0];
        np_buf[17] = ev_first[2];
        np_buf[18] = ev_first[3];
    }
    else if (ack && sub != 5)
    {
        const uint16_t *tbl = (sub == 1) ? pb : pa;
        for (i = 0; i < 12; i++)
            np_buf[7 + i] = nfc_rd(tbl[i]);
    }
    nfc_close();
}

void nfc_probe_fill(uint8_t *r)
{
    uint8_t i;
    for (i = 0; i < 20; i++)
        r[i] = np_buf[i];
    r[1] = 0;
    r[3] = gpio_read(NFC_IRQ) ? 1 : 0;
    r[4] = np_fall;
    r[5] = np_rise;
    r[6] = np_seen;
    r[19] = 0x5A;
}
