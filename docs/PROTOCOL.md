# Bluetooth protocol

Service `13187b10-eba9-a3ba-044e-83d3217d9a38`, characteristic `4b646063-6264-f3a7-8941-e65356ea82fe` (read and write, with response).
**Write:** a command byte and its arguments, at most 20 bytes (the phone may negotiate a larger MTU, but the receive buffers of this firmware are sized for 20-byte writes; writes without response were tried and make the tag stop answering, so they are not offered).
**Read:** 20 bytes, the status after the last command; byte 19 is always `0x5A` (anything else is a stale or empty read: read again).
Advertising: the packet carries the short name `badge154`, the scan response the full name `badge154-<name>` (or `badge154-` and the last three address bytes).

## Commands

| Cmd | Arguments | Meaning |
|---|---|---|
| 0x10 | pattern, seq, flags | draw the test pattern on the panel (diagnostics) |
| 0x20 | slot, flags, len (2, LE), crc16 (2, LE), pal0, pal1 | begin an upload. flags: bit 0 = LZ4, bit 1 = 1 bit per pixel. The tag asks the phone for a short connection interval |
| 0x21 | data (1 to 19 bytes) | upload data, in order |
| 0x22 | | end: the tag checks the length and the CRC and only then makes the slide visible |
| 0x23 | slot | delete a slide |
| 0x24 | | delete all slides |
| 0x25 | slot | show a slide (panel refresh, about 25 s) |
| 0x26 | interval (2, minutes, LE), order (0 in order, 1 random), flags | settings. flags: bit 0 = NFC tap disabled. Keeps the other stored settings |
| 0x27 | 0 = next, 1 = previous | step |
| 0x28 | | info (refreshes the status) |
| 0x29 | | list: bytes 2 to 16 of the answer are a bitmap of the used slots |
| 0x2A | | abort the upload |
| 0x2B | sub, arg | NFC chip diagnostics (answer in a separate format) |
| 0x2C | page 0 to 4 | hardware report (see below) |
| 0x2D | 0xA5 | watchdog test: the tag hangs on purpose and restarts after about 8 s |
| 0x2E | | draw the hardware report on the tag screen |
| 0x2F | index 0 to 8 | radio power, stored in the flash. Index: 0 +10, 1 +7, 2 +3 (default), 3 0, 4 -3, 5 -6, 6 -10, 7 -16, 8 -19 dBm |
| 0x31 | | read the tag name: answer byte 2 = length, bytes 3 and on = the name |
| 0x32 | name (0 to 16 bytes) | set the name; no bytes = the default name. Control characters are refused |

Unknown commands give result 9. While the battery is low, commands that write (0x20 to 0x27, 0x2E, 0x2F, 0x32) give result 8 and an open upload is aborted.

## Status (answer to every command except 0x2B, 0x2C, 0x29, 0x31)

| Byte | Meaning |
|---|---|
| 0 | last command |
| 1 | result: 0 ok, 1 bad argument, 2 no space, 3 CRC, 4 busy, 5 flash, 6 not found, 7 no upload, 8 low battery, 9 unknown command |
| 2 | flags: bit 0 upload open, bit 1 panel refreshing, bit 2 show pending, bit 3 low battery |
| 3 | slide count |
| 4 | current slide (255 = none) |
| 5 | free blocks (4 KB) |
| 6-7 | interval, minutes (LE) |
| 8 | order |
| 9 | settings flags |
| 10-11 | battery, mV (LE) |
| 12 | firmware version |
| 13-14 | upload position (LE) |
| 15 | total blocks (98) |
| 16-18 | reserved (0) |
| 19 | 0x5A |

## Hardware report (0x2C, byte 2 of the answer = page)

| Page | Bytes (LE) |
|---|---|
| 0 | 3-4 battery mV, 5-8 uptime s, 9-12 seconds to the next change (0xFFFFFFFF = none), 13 radio power dBm (signed), 14-15 ATT MTU, 16-17 connection interval (1.25 ms units), 18 watchdog restarts |
| 1 | 3-5 flash JEDEC ID, 6-7 chip temperature raw, 8-9 panel temperature bytes, 10-13 age of the panel reading s, 14-17 firmware size |
| 2 | 3-6 RAM used, 7-10 RAM free, 11 slide sectors, 12 free sectors, 13 slides, 14-15 interval |
| 3 | 3-18 unique ID of the flash chip (private: show only to the owner) |
| 4 | 3-8 MAC address (private) |

## Stored format

CRC-16/CCITT-FALSE (polynomial 0x1021, initial value 0xFFFF) over the payload as sent.
Pixel codes: 0 black, 1 white, 2 yellow, 3 red; 2 bits per pixel, four pixels per byte, the first pixel in the top bits, row by row, 200 x 200 (10000 bytes raw).
A slide is one record that starts at a 4 KB sector boundary in the flash: a 16-byte header (magic `SL`, slot, flags, length, CRC, two palette codes for the 1-bit mode, a check byte) and the payload.
The header is written last, so a slide that was not finished is never taken for a slide.
Compression: LZ4 block format; the 1-bit mode maps bit 0 to `pal0` and bit 1 to `pal1`.
