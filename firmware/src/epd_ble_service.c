#include "epd_ble_service.h"

#include <stdint.h>
#include "tl_common.h"
#include "stack/ble/ble.h"

#include "epd.h"
#include "epd_bwry_154.h"
#include "slides.h"
#include "ble.h"

/* badge154: replaces the upstream epd_ble_service.c (raw image buffer commands 0x00..0x04 are gone).
 * 0x20..0x3F  slideshow protocol (slides.c)
 * 0x10        panel test pattern: 10 <pattern> <seq> <flags> */
int epd_ble_handle_write(void *p)
{
	rf_packet_att_write_t *req = (rf_packet_att_write_t *)p;
	uint8_t *payload = &req->value;
	unsigned int payload_len = req->l2capLen - 3;

	if (payload_len < 1)
		return 0;

	switch (payload[0])
	{
	case 0x20 ... 0x3F:
		return slides_ble_cmd(payload, payload_len);
	case 0x10:
		if (payload_len < 4)
			return 0;
		ble_set_connection_speed(200);
		EPD_BWRY_154_test(payload[1], payload[2], payload[3]);
		return 0;
	default:
		return 0;
	}
}
