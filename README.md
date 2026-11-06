# badge154

A standalone e-paper slideshow for the **Hanshow Nebular Pro 154Q-N** electronic shelf label (1.54", 200 x 200 px,
black / white / yellow / red, Telink TLSR8359). Slides are stored in the tag itself and are uploaded from a phone or a
computer over Bluetooth with a web page. The picture stays on the e-paper without power.

**Control page:** https://steanlab.github.io/badge154/  (Chrome on Android or a computer; installable as an app, works offline after the first visit)

> **Status: early.** The panel driver is verified on a real tag. The slide storage, the upload protocol and the page are
> tested against an emulator and a mock Bluetooth device; the first run of the complete firmware on a tag is in progress.
> Flashing replaces the original firmware of the tag. Use at your own risk, no warranty.

## What it does

- Keeps many slides in the internal flash of the tag (86 blocks of 4 KB; a flat picture takes one block, a dithered photo one to three).
- Compresses every slide (raw, LZ4, 1 bit per pixel, 1 bit + LZ4: the smallest is chosen); the tag checks a CRC before a slide becomes visible.
- Changes slides automatically every N minutes (default 60, 0 = off), in order or at random; settings are kept in flash.
- The page prepares pictures: crop / fit, rotate, mirror, contrast, gamma, dithering to the four ink colours or to black and white.
- Lists the slides on the tag; show, delete, previous / next, delete all.

Not done yet: NFC tap to change slides, buttons, low-power tuning, removal of the upstream features that this slideshow does not need.

## Repository

| Path | What |
|---|---|
| `docs/` | the control page (GitHub Pages), the installable-app files, and [`HARDWARE.md`](docs/HARDWARE.md): what is known about the chip, the pins and the panel, and how sure that is |
| `firmware/` | the firmware overlay: new files and a patch script for [atc1441/ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper) (see [`firmware/README.md`](firmware/README.md)) |
| `tests/` | host tests: the real `slides.c` against a flash emulator, the JS codec of the page against the C code (`bash tests/run.sh`) |
| `.github/workflows/build.yml` | runs the tests and builds the firmware file |

## Using it

1. Flash the firmware (see `firmware/README.md`; back up the stock flash first).
2. Open the page in Chrome (Android, Windows, macOS, Linux). iPhone: Safari has no Web Bluetooth.
3. **Connect**, pick the tag (`badge154`).
4. Choose pictures, look at the preview and the size line, **Upload to the tag**.
5. Set the interval and the order, **Save to tag**.

To install on Android: open the page in Chrome and press **Install app** (or the Chrome menu, "Install app").

## Bluetooth protocol

Service `13187b10-eba9-a3ba-044e-83d3217d9a38`, characteristic `4b646063-6264-f3a7-8941-e65356ea82fe`.
Write: command byte + arguments (at most 20 bytes). Read: 20 bytes, the status of the last command (the byte 19 is always 0x5A).

| Cmd | Arguments | Meaning |
|---|---|---|
| 0x20 | slot, flags, len (2), crc16 (2), pal0, pal1 | begin an upload (flags: bit 0 = LZ4, bit 1 = 1 bit per pixel) |
| 0x21 | data, up to 19 bytes | upload data |
| 0x22 | | end: the tag checks the CRC and only then makes the slide visible |
| 0x23 | slot | delete a slide |
| 0x24 | | delete all slides |
| 0x25 | slot | show a slide |
| 0x26 | interval (2, minutes), order (0 / 1), flags | settings |
| 0x27 | 0 = next, 1 = previous | step |
| 0x28 | | info |
| 0x29 | | list: bytes 2..16 of the status are a bitmap of the used slots |
| 0x2A | | abort the upload |

Status bytes: 0 last command, 1 result (0 = ok, 1 bad argument, 2 no space, 3 CRC, 4 busy, 5 flash, 6 not found, 7 no upload, 8 low battery, 9 unknown command),
2 flags (bit 0 upload active, bit 1 panel refreshing, bit 2 show pending), 3 slide count, 4 current slide, 5 free blocks, 6-7 interval, 8 order,
9 flags, 10-11 battery in mV, 12 firmware version, 13-14 upload position, 15 total blocks.

CRC-16/CCITT-FALSE (polynomial 0x1021, initial value 0xFFFF) over the stored payload. Pixel codes: 0 black, 1 white, 2 yellow, 3 red;
2 bits per pixel, four pixels per byte, first pixel in the top bits, row by row, 200 x 200.
Stored record (4 KB aligned): 16-byte header (magic `SL`, slot, flags, length, CRC, two palette codes, check byte) + payload; the header is written last.

## Credits and licence

MIT licence, see [`LICENSE`](LICENSE). The firmware builds on atc1441's [ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper);
the panel information was read from atc1441's ATC_BLE_OEPL. Details and third-party licences: [`NOTICE.md`](NOTICE.md).
A similar project for another tag: [badge27](https://github.com/steanlab/badge27).
