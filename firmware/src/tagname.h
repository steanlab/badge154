#pragma once
#include <stdint.h>

/* Name of the tag in the Bluetooth device list: "badge154-" + the name given by the user, or "badge154-XXXXXX"
 * (the last three MAC bytes) while no name is set.  The name is kept with the other settings (slides.c). */
#define TAGNAME_MAX 16 /* bytes (UTF-8 is fine, no control characters) */

void tagname_apply(const uint8_t *name, uint8_t len); /* len 0 = the default name; updates the scan response at once */
