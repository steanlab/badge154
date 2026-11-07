#include "host_stubs.h"
#include <stdlib.h>
#include <stdio.h>
#include "hwinfo.h"
uint8_t flash_mem[512*1024];

uint16_t battery_mv = 3000;
uint8_t epd_update_state = 0;
uint32_t fake_time = 0;
int fake_conn = 0;
uint8_t shown[10000]; int show_count = 0;
int write_violations = 0, erase_count = 0, region_violations = 0;
/* the firmware may program and erase only the slide area and the settings sector (the rest holds the firmware, the MAC address and the calibration) */
static int region_ok(uint32_t a){ return (a >= 0x14000u && a < 0x76000u) || (a >= 0x79000u && a < 0x7A000u); }
void flash_read_page(uint32_t a, uint32_t l, uint8_t *b){ memcpy(b, flash_mem+a, l); }
/* The real flash cannot be read while it is programmed, so the source of a write must be in RAM (writable), never const data.
   Checked through /proc/self/maps: the source mapping has to be writable. */
static int src_writable(const void *p){
  FILE *f = fopen("/proc/self/maps", "r"); char line[512]; int ok = 0;
  if (!f) return 1;
  while (fgets(line, sizeof line, f)) {
    unsigned long lo, hi; char perm[8];
    if (sscanf(line, "%lx-%lx %7s", &lo, &hi, perm) == 3 && (unsigned long)p >= lo && (unsigned long)p < hi) { ok = perm[1] == 'w'; break; }
  }
  fclose(f); return ok;
}
void flash_write_page(uint32_t a, uint32_t l, uint8_t *b){
  if (l <= 16 && !src_writable(b)) write_violations++;
  if ((a & 255) + l > 256) write_violations++;
  if (!region_ok(a) || !region_ok(a + l - 1)) region_violations++;
  for (uint32_t i=0;i<l;i++){ if ((flash_mem[a+i] & b[i]) != b[i]) write_violations++; flash_mem[a+i] &= b[i]; }
}
void flash_erase_sector(uint32_t a){ if (a & 4095) write_violations++; if (!region_ok(a)) region_violations++; memset(flash_mem + a, 0xFF, 4096); erase_count++; }
uint32_t get_time(void){ return fake_time; }
uint32_t clock_time(void){ return 777; }
int ble_get_connected(void){ return fake_conn; }
void ble_set_connection_speed(uint16_t s){ (void)s; }
void EPD_BWRY_154_show(const uint8_t *img){ memcpy(shown, img, 10000); show_count++; }
void host_reset(void){ memset(flash_mem,0xFF,sizeof flash_mem); }
int fake_tap = 0;
void nfc_poll(void){}
uint8_t nfc_tap_take(void){ uint8_t t = (uint8_t)fake_tap; fake_tap = 0; return t; }
int tx_stub = 2;
void txpower_set(uint8_t i){ if (i < 9) tx_stub = i; }
int8_t txpower_dbm(void){ return 3; }
int wdt_test_calls = 0;
void hwinfo_text(char (*ln)[TXT_COLS + 1], const hw_slides_t *s){ int i; (void)s; for(i=0;i<TXT_LINES;i++) strcpy(ln[i], i==0 ? "Battery 3.012 V" : i==5 ? "RAM 19659 B free" : "TEST LINE"); }
void hwinfo_low_text(char (*ln)[TXT_COLS + 1], uint16_t mv){ (void)mv; strcpy(ln[0], "Battery 2.35 V"); strcpy(ln[1], "Updates paused"); strcpy(ln[2], "Slides are kept"); strcpy(ln[3], "Change battery"); }
uint16_t get_battery_mv(void){ return battery_mv; }
void wdt_test_request(void){ wdt_test_calls++; }
uint8_t wdt_restarts(void){return 0;}
void nfc_probe_run(uint8_t s, uint8_t a){(void)s;(void)a;}
void nfc_probe_fill(uint8_t *r){ r[0]=0x2B; r[1]=0; r[19]=0x5A; }
void led_link_tick(void){}
uint8_t led_link_active(void){ return 0; }
void hwinfo_fill(uint8_t page, uint8_t *r, const hw_slides_t *s){ memset(r,0,20); r[0]=0x2C; r[2]=page; r[3]=s->sectors; r[4]=s->free_sectors; r[5]=s->count; r[6]=(uint8_t)s->interval_min; memcpy(r+7,&s->next_s,4); r[19]=0x5A; }
