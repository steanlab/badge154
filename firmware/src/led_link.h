#pragma once
#include <stdint.h>

/* RGB light show while a phone is connected: red -> green -> blue -> red, smooth cross-fades.
 * led_link_tick()   - call as often as possible from the main loop
 * led_link_active() - 1 while the show runs (the CPU must not sleep then, the fades are software PWM) */
void led_link_tick(void);
uint8_t led_link_active(void);
