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

for f in ('epd_bwry_154.c', 'epd_bwry_154.h', 'slides.c', 'slides.h', 'splash.h', 'nfc_probe.c', 'nfc_probe.h',
          'epd.c', 'epd.h', 'epd_ble_service.c', 'project.mk', 'nfc.c', 'led_link.c', 'led_link.h', 'hwinfo.c', 'hwinfo.h', 'wdt.c', 'wdt.h', 'textscr.c', 'textscr.h', 'txpower.c', 'txpower.h'):  # epd.c .. nfc.c replace the upstream files
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

# --- app.c: no clock any more; slideshow instead ---
edit('app.c', r'#include "bart_tif.h"\n', '')
edit('app.c', r'#include "time.h"', '#include "time.h"\n#include "slides.h"')
edit('app.c', r'(    init_flash\(\);\n)', r'\1    slides_init();\n')
edit('app.c', r'(    handler_time\(\);\n)', r'\1    slides_tick();\n')
edit('app.c', r'    uint8_t current_minute = .*?(    if \(time_reached_period\(Timer_CH_0, 10\)\))', r'\1', flags=re.S)

# --- ble.c: the old text command parser (settings of the thermometer firmware) is not needed ---
edit('ble.c', r'\tcmd_parser\(p\);\n', '')

# --- no UART log and no OTA: the UART pin (PB1) stays untouched (also while a programmer is attached), and the OTA service of the
#     upstream firmware (it erases and writes 0x20000.. = the slide area, reboots, rewrites the firmware) answers nothing ---
edit('main.c', r'\tinit_uart\(\);\n', '')
edit('main.c', r'\t+printf\([^\n]*\);\n', '', count=2)
edit('ble.c', r'\t+printf\([^\n]*\);\n', '', count=2)
edit('ble.c', r'\tif \(ota_started == 0\)\s*\{\s*ota_started = 1;\s*ble_set_connection_speed\(6\);\s*\}\s*return custom_otaWrite\(p\);',
     '\treturn 0; /* OTA is not supported: the slides live where the upstream OTA would write */', flags=re.S)

# --- LED: the green heartbeat flash every 30 s, only while nobody is connected (a connection has its own light show, led_link.c);
#     the CPU stays awake during the show (software PWM) ---
edit('app.c', r'if \(time_reached_period\(Timer_CH_0, 10\)\)\s*\{\s*if \(ble_get_connected\(\)\)\s*set_led_color\(3\);\s*else\s*set_led_color\(2\);\s*WaitMs\(1\);\s*set_led_color\(0\);\s*\}',
     'if (time_reached_period(Timer_CH_0, 30))\n    {\n        if (!ble_get_connected())\n        {\n            set_led_color(2);\n            WaitMs(1);\n            set_led_color(0);\n        }\n    }', flags=re.S)
edit('app.c', r'(    else\n    \{\n        blt_pm_proc\(\);\n)(    \})', r'\1        if (led_link_active())\n            bls_pm_setSuspendMask(SUSPEND_DISABLE);\n\2')
edit('app.c', r'#include "slides.h"', '#include "slides.h"\n#include "led_link.h"')

# --- watchdog (wdt.c): armed at start and after every wake-up, fed at the top of the main loop ---
edit('app.c', r'#include "led_link.h"', '#include "led_link.h"\n#include "wdt.h"')
edit('app.c', r'(void user_init_normal\(void\)\n\{[^\n]*\n)', r'\1    wdt_boot();\n    wdt_init();\n')
edit('app.c', r'(    blc_ll_recoverDeepRetention\(\);\n)', r'\1    wdt_init();\n')
# --- TX power: the level comes from the settings (txpower.c), also after every sleep ---
edit('app.c', r'#include "wdt.h"', '#include "wdt.h"\n#include "txpower.h"')
edit('app.c', r'rf_set_power_level_index\(RF_POWER_P3p01dBm\);', 'txpower_apply();')
edit('ble.c', r'#include "ble.h"', '#include "ble.h"\n#include "txpower.h"')
edit('ble.c', r'rf_set_power_level_index\(RF_POWER_P3p01dBm\);', 'txpower_apply();')
edit('app.c', r'(_attribute_ram_code_ void main_loop\(void\)\n\{\n)', r'\1    wdt_feed();\n')

# --- time.c: catch up if the main loop was blocked for a while ---
edit('time.c', r'    if \(clock_time\(\) - last_clock_increase >= one_second_trimmed\)', '    while (clock_time() - last_clock_increase >= one_second_trimmed)')

# --- app_att.c: 20 byte readable status in the EPD_BLE characteristic ---
edit('app_att.c', r'static u8 \t  my_EPD_BLE_Data \t+= 0x00;', 'extern u8 my_EPD_BLE_Data[20];')
edit('app_att.c', r'\{0,ATT_PERMISSIONS_WRITE, 16, sizeof\(my_EPD_BLE_Data\), \(u8\*\)\(&my_EPD_BLEUUID\),\t\(&my_EPD_BLE_Data\),',
     '{0,ATT_PERMISSIONS_RDWR, 16, sizeof(my_EPD_BLE_Data), (u8*)(&my_EPD_BLEUUID),\t(u8*)my_EPD_BLE_Data,')

# --- Bluetooth name: "badge154-XXXXXX" (XXXXXX = last three MAC bytes, as upstream). The full name is in the scan response;
#     the advertising packet carries the shortened name "badge154" so that name filters of browsers match reliably ---
edit('ble.c', r"RAM uint8_t ble_name\[\] = \{11, 0x09, 'E', 'S', 'L', '_', '0', '0', '0', '0', '0', '0'\};",
     "RAM uint8_t ble_name[] = {16, 0x09, 'b', 'a', 'd', 'g', 'e', '1', '5', '4', '-', '0', '0', '0', '0', '0', '0'};")
edit('ble.c', r"(hex_ascii = \{\"0123456789ABCDEF\"\};\n)\tble_name\[6\](.*?)\tble_name\[7\](.*?)\tble_name\[8\](.*?)\tble_name\[9\](.*?)\tble_name\[10\](.*?)\tble_name\[11\]",
     r"\1\tble_name[11]\2\tble_name[12]\3\tble_name[13]\4\tble_name[14]\5\tble_name[15]\6\tble_name[16]", flags=re.S)
edit('ble.c', r"(/\*Counter\*/ 0x00)\};",
     r"\1,\n\t/*Name*/ 9, 0x08, 'b', 'a', 'd', 'g', 'e', '1', '5', '4',\n\t/*Flags*/ 2, 0x01, 0x06};")
print("all edits applied")
