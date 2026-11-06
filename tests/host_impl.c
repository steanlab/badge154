#include "host_stubs.h"
#include <stdlib.h>
uint8_t flash_mem[512*1024];

uint16_t battery_mv = 3000;
uint8_t epd_update_state = 0;
uint32_t fake_time = 0;
int fake_conn = 0;
uint8_t shown[10000]; int show_count = 0;
int write_violations = 0, erase_count = 0;
void flash_read_page(uint32_t a, uint32_t l, uint8_t *b){ memcpy(b, flash_mem+a, l); }
void flash_write_page(uint32_t a, uint32_t l, uint8_t *b){
  if ((a & 255) + l > 256) write_violations++;
  for (uint32_t i=0;i<l;i++){ if ((flash_mem[a+i] & b[i]) != b[i]) write_violations++; flash_mem[a+i] &= b[i]; }
}
void flash_erase_sector(uint32_t a){ if (a & 4095) write_violations++; memset(flash_mem + a, 0xFF, 4096); erase_count++; }
uint32_t get_time(void){ return fake_time; }
uint32_t clock_time(void){ return 777; }
int ble_get_connected(void){ return fake_conn; }
void ble_set_connection_speed(uint16_t s){ (void)s; }
void EPD_BWRY_154_show(const uint8_t *img){ memcpy(shown, img, 10000); show_count++; }
void host_reset(void){ memset(flash_mem,0xFF,sizeof flash_mem); }
int fake_tap = 0;
void nfc_poll(void){}
uint8_t nfc_tap_take(void){ uint8_t t = (uint8_t)fake_tap; fake_tap = 0; return t; }
void nfc_probe_run(uint8_t s, uint8_t a){(void)s;(void)a;}
void nfc_probe_fill(uint8_t *r){ r[0]=0x2B; r[1]=0; r[19]=0x5A; }
