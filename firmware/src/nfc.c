#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "stack/ble/ble.h"
#include "drivers/8258/flash.h"
#include "nfc.h"
#include "main.h"
#include "i2c.h"

/* badge154: replaces the upstream nfc.c.
 * The upstream code wrote EEPROM cell 0x03B5 of the FM11NT083V (value 0xA0) on every start.
 * Here the cell is read first and written only when it differs: no needless EEPROM wear, a faster start. */
#define NFC_ID   0xAE
#define NFC_CFG_ADDR 0x03B5
#define NFC_CFG_VAL  0xA0

_attribute_ram_code_ void init_nfc(void)
{
    uint8_t v = 0;

    gpio_write(NFC_CS, 1);
    gpio_set_func(NFC_CS, AS_GPIO);
    gpio_set_output_en(NFC_CS, 1);
    gpio_set_input_en(NFC_CS, 0);
    gpio_setup_up_down_resistor(NFC_CS, PM_PIN_PULLUP_10K);

    gpio_set_func(NFC_IRQ, AS_GPIO);
    gpio_set_output_en(NFC_IRQ, 0);
    gpio_set_input_en(NFC_IRQ, 1);
    gpio_setup_up_down_resistor(NFC_IRQ, PM_PIN_PULLUP_10K);

    gpio_write(NFC_CS, 0);
    sleep_us(500);
    i2c_set_id(NFC_ID);
    i2c_read_series(NFC_CFG_ADDR, 2, &v, 1);
    if (v != NFC_CFG_VAL)
    {
        uint8_t w[3] = {NFC_CFG_ADDR >> 8, NFC_CFG_ADDR & 0xFF, NFC_CFG_VAL};
        send_i2c(NFC_ID, w, sizeof(w));
        sleep_ms(10); /* EEPROM write cycle */
    }
    gpio_write(NFC_CS, 1);
}
