# Hardware notes: Hanshow Nebular Pro 154Q-N (TLSR8359, 1.54" BWRY)

What is known about the tag, and how sure I am about it. Three levels are used:

- **verified**: seen working on a real tag in this project,
- **read from firmware**: taken from the disassembly of the published firmware ATC_BLE_OEPL (preset "38: 200 HS BWRY JD"), not independently measured,
- **assumed / unknown**: not checked.

The tag is identified as 154QN, hardware ID 0x006C in the OpenEPaperLink tag-type table (read from firmware).

## Processor

| | |
|---|---|
| Chip | Telink TLSR8359 (the TLSR8258 family: 32-bit TC32 core, BLE 5 radio) (verified: runs the ATC_TLSR_Paper firmware built with the TLSR8258 SDK) |
| Flash | 512 KB, **internal**; I found no external flash chip in the firmware's pin configuration (read from firmware). Storing slides in the internal flash is tested in an emulator only, **not yet on the tag** |
| RAM | 64 KB SRAM from 0x840000. Deep-retention sleep keeps only the lower 32 KB (`DEEPSLEEP_MODE_RET_SRAM_LOW32K`) |
| Toolchain | Telink `tc32` GCC 4.5.1 (shipped in the upstream repository; runs on x86-64 Linux, 32-bit libraries may be needed) |
| Programming | wired UART boot loader, flashed with the WebSerial flasher page of atc1441 (verified) |
| UART pins of the firmware | RXD = PA0, TXD = PB1 (from the upstream `main.h`) |

### Flash map used by badge154

| Address | Use |
|---|---|
| 0x00000 - 0x1FFFF | firmware (about 93 KB used now) |
| 0x20000 - 0x75FFF | slides: 86 sectors of 4 KB |
| 0x76000 - 0x77FFF | left alone: by Telink convention the MAC address and calibration data live here |
| 0x78000 - 0x78FFF | settings sector of the upstream firmware (it writes at 0x78100) |
| 0x79000 | slideshow settings (interval, order) |

Note for the OTA feature of the upstream firmware: a Telink OTA image is written at 0x20000, i.e. over the slides.
Do not use OTA with this firmware; flash over UART.

### Things learned about the SDK and the upstream firmware

- Variables that must survive deep-retention sleep need the `RAM` attribute (section `.retention_data`). Ordinary statics and
  `.bss` are not kept. The retention section ends at about 0x8464E4 and may grow up to 0x848000.
- Flash API: `flash_read_page`, `flash_write_page` (at most 256 bytes, must not cross a 256-byte page), `flash_erase_sector` (4 KB).
  Writes only clear bits; erase before write.
- BLE: the GATT table of the upstream firmware has the characteristic `4b646063-6264-f3a7-8941-e65356ea82fe` in the service
  `13187b10-eba9-a3ba-044e-83d3217d9a38`. The attribute has a write callback. The receive FIFO is 64 x 8 bytes, so writes are
  kept to 20 bytes (the default ATT MTU of 23). The advertised name is `ESL_xxxxxx`.
- Connection parameters are in units of 1.25 ms; the upstream firmware asks for 200 (250 ms) to save power; the uploader asks for 16 (20 ms).
- Time: `get_time()` counts seconds from the 16 MHz system timer with a fixed trim constant (`time_trime = 5000`). Its accuracy over
  hours has not been measured.
- During a panel refresh the main loop disables CPU suspend (it waits for BUSY).

## Pin assignment

| Function | Pin | Notes |
|---|---|---|
| Panel RESET | PD4 | |
| Panel DC (data / command) | PB7 | |
| Panel BUSY | PA1 | **high = idle, low = busy** (read from firmware; verified: refresh completes) |
| Panel CS | PD2 | |
| Panel CLK | PD7 | software SPI (bit-banged), MSB first; CS is pulsed low for every single byte (command or data) |
| Panel MOSI | PB6 | also used as input for the 2-byte read after command 0x40 |
| Panel power ENABLE | PB5 | **driven low = panel powered**; when off it is an input with a 1 MΩ pull-up (verified) |
| LED red / green / blue | PB4 / PD3 / PA7 | active low (read from firmware; a blinking LED was confirmed, the colour of each pin was not checked one by one) |
| Battery ADC | PB3 | read from firmware; the voltage scaling of the upstream firmware is used as is and has **not** been checked against a multimeter |
| NFC chip FM11NT083V | SDA PC0, SCL PC1, CS PC6, IRQ PC4 | read from firmware; **not used yet** |

## Display

| | |
|---|---|
| Size | 200 x 200 pixels, 1.54", four colours: black, white, yellow, red |
| Controller | a four-colour controller of the "JD" family (name taken from the OpenEPaperLink driver preset "200 HS BWRY JD"). The exact chip marking is not known. It is **not** compatible with the black/white/red controllers of the upstream firmware |
| Interface | 4-wire SPI + DC + RESET + BUSY, write only except for the 2-byte read after command 0x40 |
| Pixel format | 2 bits per pixel, 4 pixels per byte, first pixel in the top bits, rows from top to bottom, 50 bytes per row, 10000 bytes per image (verified) |
| Colour codes | **0 = black, 1 = white, 2 = yellow, 3 = red** (verified with a test image) |
| Orientation | in the image as sent, x grows to the right and y grows down (verified with a test image: black square top-left, yellow top-right, red bottom-left, white elsewhere, as seen by the person looking at the panel). How this relates to the tag body (which edge carries the NFC antenna or the label) was not recorded |
| Refresh | full refresh only; takes roughly 20 to 30 seconds (BUSY stays low meanwhile; not timed precisely). Partial refresh: unknown, not used |

The diagnostic BLE command of the firmware (`10 06 00 00`) draws that test image again, so you can check your own tag.

### Power-up, init and refresh sequence (the one used by `firmware/src/epd_bwry_154.c`)

Verified on the tag: with this exact sequence the panel shows the image.

1. All panel pins as GPIO; CS, DC, CLK, RESET high. Wait 5 ms.
2. ENABLE: output, low (panel power on). Wait 5 ms.
3. RESET low 20 ms, high 20 ms. Wait 40 ms. Wait for BUSY high (timeout 400 ms).
4. Commands (command byte, then data bytes; DC low for the command byte, high for data):

```
00: 0F 29
01: 07 00 22 78 0A 22
03: 10 54 44
06: C0 C0 C0
41: 00
50: 37
60: 02 02
E7: 1C
E3: 22
E9: 01
41: 00
40            (temperature sensor), wait BUSY high, then read 2 bytes (DC high, 8 clocks each)
FF: A5
EF: 03 44 07 32 07 16 09 33
DC: 01
DD: 06
DE: 70
E8: 00
DA: 00
FF: E3
61: 00 C8 00 C8   (resolution 200 x 200)
65: 00 00 00 00   (window offset)
04            (power on), wait BUSY high (timeout 400 ms)
10            (start of the image data), then 10000 data bytes
12 00         (refresh), then wait for BUSY high (20 to 30 s)
```

5. After the refresh: `02 00` (power off), wait BUSY high, `07 A5` (deep sleep), then ENABLE back to input with pull-up (panel power off).

Unknown: whether the temperature read (`40` plus two bytes) is needed (it is done as the reference firmware does; the result is not used),
what the registers `EF`, `DC`..`DE`, `E8`, `DA` (waveform and voltage settings, read from firmware) do exactly, and whether other
temperatures need different values. All tests were done at room temperature.

The stock Hanshow firmware contains initialisation data for several panel controllers. Its stream for this panel was only partly
decoded and is not used here.

## Not done / open

- NFC (FM11NT083V over I2C) is not used.
- Battery voltage scale not checked; the low-battery limit used for flash writes (2.3 V) is a guess.
- Real power consumption (sleep current, refresh energy) not measured.
- Colour accuracy: the control page uses nominal RGB values for the four ink colours; the real panel colours were not measured.
