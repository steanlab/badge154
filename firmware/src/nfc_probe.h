#pragma once
#include <stdint.h>

/* NFC diagnostics for the FM11NT083V (I2C 0xAE, IRQ on PC4).
 * nfc_poll()       - call often: counts level changes of the IRQ pin
 * nfc_probe_run()  - BLE command 0x2B <sub> <arg>: reads / writes chip registers into an answer
 * nfc_probe_fill() - copies that answer (plus the live IRQ level and counters) into the 20-byte status */
void nfc_poll(void);
void nfc_probe_run(uint8_t sub, uint8_t arg);
void nfc_probe_fill(uint8_t *r);
/* nfc_tap_take() - 1 once per phone tap (field on / tag selected after >= 3 s without any NFC event), then 0 */
uint8_t nfc_tap_take(void);
