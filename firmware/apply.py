#!/usr/bin/env python3
"""badge154 firmware overlay for atc1441/ATC_TLSR_Paper (Hanshow Nebular Pro 154, BWRY panel).

Usage:  python3 firmware/apply.py <path to a checkout of atc1441/ATC_TLSR_Paper>
Pinned upstream commit: 3778d296f418c30b05310d86dafa4e3404071cb4
Copies the files of firmware/src into <checkout>/Firmware/src and patches the upstream sources.
Every edit must match exactly once, otherwise the script stops with an error."""
import re, shutil, sys, pathlib

if len(sys.argv) != 2:
    sys.exit(__doc__)
root = pathlib.Path(sys.argv[1]) / 'Firmware' / 'src'
here = pathlib.Path(__file__).resolve().parent / 'src'

def edit(name, pattern, repl, regex=True, count=1, flags=0):
    p = root / name
    s = p.read_text()
    found = len(re.findall(pattern, s, flags)) if regex else s.count(pattern)
    if found != count:
        sys.exit(f"ERROR {name}: pattern {pattern!r} found {found} times, expected {count}")
    s = re.sub(pattern, lambda m: m.expand(repl), s, flags=flags) if regex else s.replace(pattern, repl)
    p.write_text(s)
    print("ok", name, pattern[:50].replace("\n", " "))

for f in ('epd_bwry_154.c', 'epd_bwry_154.h', 'slides.c', 'slides.h'):
    shutil.copy(here / f, root / f)

# --- pinout of the Hanshow Nebular Pro 154 (ATC_BLE_OEPL preset "200 HS BWRY JD") ---
edit('main.h', r'(#define LED_RED\s+)GPIO_PD2', r'\1GPIO_PB4')
edit('main.h', r'(#define EPD_DC\s+)GPIO_PD7', r'\1GPIO_PB7')
edit('main.h', r'(#define EPD_CS\s+)GPIO_PB4', r'\1GPIO_PD2')
edit('main.h', r'(#define EPD_CLK\s+)GPIO_PB5', r'\1GPIO_PD7')
edit('main.h', r'(#define EPD_ENABLE\s+)GPIO_PC5', r'\1GPIO_PB5')
edit('battery.c', r'adc_vbat_init\(GPIO_PB7\)', 'adc_vbat_init(GPIO_PB3)')

# --- panel power: ENABLE output LOW = on; off = input with pullup 1M ---
edit('epd_spi.h', '#define EPD_POWER_ON() gpio_write(EPD_ENABLE, 0)',
     '#define EPD_POWER_ON() do { gpio_set_input_en(EPD_ENABLE, 0); gpio_write(EPD_ENABLE, 0); gpio_set_output_en(EPD_ENABLE, 1); gpio_setup_up_down_resistor(EPD_ENABLE, PM_PIN_PULLDOWN_100K); } while (0)', regex=False)
edit('epd_spi.h', '#define EPD_POWER_OFF() gpio_write(EPD_ENABLE, 1)',
     '#define EPD_POWER_OFF() do { gpio_set_output_en(EPD_ENABLE, 0); gpio_set_input_en(EPD_ENABLE, 1); gpio_setup_up_down_resistor(EPD_ENABLE, PM_PIN_PULLUP_1M); } while (0)', regex=False)

edit('project.mk', r'(\$\(OUT_PATH\)/epd_bwr_154\.o \\\n)', r'\1$(OUT_PATH)/epd_bwry_154.o \\\n$(OUT_PATH)/slides.o \\\n')

# --- epd.c: panel model 7 = BWRY 1.54 ---
edit('epd.c', r'#include "epd_bwr_154.h"', '#include "epd_bwr_154.h"\n#include "epd_bwry_154.h"')
edit('epd.c', r'RAM uint8_t epd_model = 0;', 'RAM uint8_t epd_model = 7;')
edit('epd.c', r'"BWY350"\}', '"BWY350", "BWRY154"}')
edit('epd.c', r'(uint8_t EPD_read_temp\(void\)\s*\{)', r'\1\n    if (epd_model == 7)\n        return 20;')
edit('epd.c', r'(epd_temperature = EPD_BWY_350_Display\(image, size, full_or_partial\);)',
     r'\1\n    else if (epd_model == 7)\n        epd_temperature = EPD_BWRY_154_Display(image, size, full_or_partial);')
edit('epd.c', r'(EPD_BWY_350_set_sleep\(\);)', r'\1\n    else if (epd_model == 7)\n        EPD_BWRY_154_set_sleep();')
edit('epd.c', r'if \(epd_model == 1\)(\s*)\{(\s*)if \(!EPD_IS_BUSY\(\)\)',
     'if (epd_model == 7)\n        {\n            if (EPD_BWRY_154_done())\n                epd_set_sleep();\n        }\n        else if (epd_model == 1)\\1{\\2if (!EPD_IS_BUSY())')

# --- app.c: no clock any more; slideshow instead ---
edit('app.c', r'#include "time.h"', '#include "time.h"\n#include "slides.h"')
edit('app.c', r'(    init_flash\(\);\n)', r'\1    slides_init();\n')
edit('app.c', r'(    handler_time\(\);\n)', r'\1    slides_tick();\n')
edit('app.c', r'    uint8_t current_minute = .*?(    if \(time_reached_period\(Timer_CH_0, 10\)\))', r'\1', flags=re.S)

# --- time.c: catch up if the main loop was blocked for a while ---
edit('time.c', r'    if \(clock_time\(\) - last_clock_increase >= one_second_trimmed\)', '    while (clock_time() - last_clock_increase >= one_second_trimmed)')

# --- app_att.c: 20 byte readable status in the EPD_BLE characteristic ---
edit('app_att.c', r'static u8 \t  my_EPD_BLE_Data \t+= 0x00;', 'extern u8 my_EPD_BLE_Data[20];')
edit('app_att.c', r'\{0,ATT_PERMISSIONS_WRITE, 16, sizeof\(my_EPD_BLE_Data\), \(u8\*\)\(&my_EPD_BLEUUID\),\t\(&my_EPD_BLE_Data\),',
     '{0,ATT_PERMISSIONS_RDWR, 16, sizeof(my_EPD_BLE_Data), (u8*)(&my_EPD_BLEUUID),\t(u8*)my_EPD_BLE_Data,')

# --- BLE command routing ---
edit('epd_ble_service.c', r'#include "epd.h"', '#include "epd.h"\n#include "epd_bwry_154.h"\n#include "slides.h"')
edit('epd_ble_service.c', r'(\tcase 0x04:)',
     '\t// slideshow commands\n\tcase 0x20 ... 0x2F:\n\t\treturn slides_ble_cmd(payload, payload_len);\n\t// BWRY test: 10 <pattern> <seq> <flags>\n\tcase 0x10:\n\t\tASSERT_MIN_LEN(payload_len, 4);\n\t\tble_set_connection_speed(200);\n\t\tEPD_BWRY_154_test(payload[1], payload[2], payload[3]);\n\t\treturn 0;\n\\1')

# --- Bluetooth name: "badge154-XXXXXX" (XXXXXX = last three MAC bytes, as upstream). The full name is in the scan response;
#     the advertising packet carries the shortened name "badge154" so that name filters of browsers match reliably ---
edit('ble.c', r"RAM uint8_t ble_name\[\] = \{11, 0x09, 'E', 'S', 'L', '_', '0', '0', '0', '0', '0', '0'\};",
     "RAM uint8_t ble_name[] = {16, 0x09, 'b', 'a', 'd', 'g', 'e', '1', '5', '4', '-', '0', '0', '0', '0', '0', '0'};")
edit('ble.c', r"(hex_ascii = \{\"0123456789ABCDEF\"\};\n)\tble_name\[6\](.*?)\tble_name\[7\](.*?)\tble_name\[8\](.*?)\tble_name\[9\](.*?)\tble_name\[10\](.*?)\tble_name\[11\]",
     r"\1\tble_name[11]\2\tble_name[12]\3\tble_name[13]\4\tble_name[14]\5\tble_name[15]\6\tble_name[16]", flags=re.S)
edit('ble.c', r"(/\*Counter\*/ 0x00)\};",
     r"\1,\n\t/*Name*/ 9, 0x08, 'b', 'a', 'd', 'g', 'e', '1', '5', '4',\n\t/*Flags*/ 2, 0x01, 0x06};")
print("all edits applied")
