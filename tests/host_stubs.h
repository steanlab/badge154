#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define RAM
void flash_read_page(uint32_t addr, uint32_t len, uint8_t *buf);
void flash_write_page(uint32_t addr, uint32_t len, uint8_t *buf);
void flash_erase_sector(uint32_t addr);
uint32_t get_time(void);
uint32_t clock_time(void);
int ble_get_connected(void);
void ble_set_connection_speed(uint16_t s);
void EPD_BWRY_154_show(const uint8_t *img);
