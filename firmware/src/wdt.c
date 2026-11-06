#include <stdint.h>
#include "tl_common.h"
#include "drivers.h"
#include "wdt.h"

/* The two analog registers DEEP_ANA_REG0/1 (0x3a, 0x3b) keep their value through a watchdog or pin reset
 * and are cleared only by a power cycle (see drivers/8258/pm.h). */
#define ANA_RUN_MARK 0x3a /* 0xA5 = the firmware has been started since the power-up */
#define ANA_RESTARTS 0x3b /* number of restarts since the power-up */
#define RUN_MARK     0xA5

RAM static uint8_t test_hang = 0;

void wdt_boot(void)
{
    if (analog_read(ANA_RUN_MARK) == RUN_MARK)
    {
        uint8_t n = analog_read(ANA_RESTARTS);
        if (n < 255)
            analog_write(ANA_RESTARTS, (uint8_t)(n + 1));
    }
    analog_write(ANA_RUN_MARK, RUN_MARK);
}

void wdt_init(void)
{
    wd_set_interval_ms(WDT_MS, CLOCK_SYS_CLOCK_1MS);
    wd_clear();
    wd_start();
}

_attribute_ram_code_ void wdt_feed(void)
{
    if (test_hang)
        for (;;)
            ; /* deliberate hang: the watchdog has to restart the tag */
    wd_clear();
    wd_start(); /* also re-arms it if some sleep code stopped it */
}

uint8_t wdt_restarts(void)
{
    return analog_read(ANA_RESTARTS);
}

void wdt_test_request(void)
{
    test_hang = 1;
}
