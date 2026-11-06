#pragma once
#include <stdint.h>

/* Watchdog: the main loop feeds it; if the firmware hangs for longer than WDT_MS, the chip restarts.
 *
 * wdt_boot()  - call once at the first start after a power-up (not after a deep sleep): counts the restarts
 * wdt_init()  - arm the watchdog (call after every start and every wake-up from deep sleep)
 * wdt_feed()  - call at the top of the main loop (and in long waits)
 * wdt_restarts() - restarts since the battery was inserted (watchdog, reset pin or a programmer); saturates at 255
 * wdt_test_request() - the next wdt_feed() hangs on purpose: the watchdog must restart the tag after WDT_MS */
#define WDT_MS 8000

void wdt_boot(void);
void wdt_init(void);
void wdt_feed(void);
uint8_t wdt_restarts(void);
void wdt_test_request(void);
