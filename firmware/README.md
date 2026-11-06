# Firmware

The firmware is the upstream [ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper) (commit
`3778d296f418c30b05310d86dafa4e3404071cb4`) with this overlay applied. The upstream code is not copied into this repository.

| File | What |
|---|---|
| `src/slides.c`, `src/slides.h` | slide storage in the flash, Bluetooth commands 0x20-0x2F, LZ4 and 1-bit decoder, settings, slideshow timer |
| `src/epd_bwry_154.c`, `src/epd_bwry_154.h` | driver of the 200 x 200 BWRY panel |
| `apply.py` | copies these files into the upstream tree and patches it: pin assignment, panel power, panel model, command routing, status characteristic, time catch-up |

## Build

On GitHub: the workflow `build` does everything and leaves `badge154-firmware.bin` (with `SHA256SUMS.txt`) as an artifact of the run.

Locally (Linux, Python 3):

```
git clone https://github.com/atc1441/ATC_TLSR_Paper
git -C ATC_TLSR_Paper checkout 3778d296f418c30b05310d86dafa4e3404071cb4
python3 firmware/apply.py ATC_TLSR_Paper
cd ATC_TLSR_Paper/Firmware
chmod +x tc32_linux/bin/*
make
```

The result is `ATC_TLSR_Paper/Firmware/ATC_Paper.bin` (about 93 KB). `apply.py` stops with an error if any of its edits does not match
exactly once, so a different upstream commit will fail loudly instead of building something wrong.

## Flash

1. Wire the tag to a USB-UART adapter as described in the upstream repository (picture `USB_UART_Flashing_connection.jpg`).
2. Open the WebSerial flasher https://atc1441.github.io/ATC_TLSR_Paper_UART_Flasher.html in Chrome or Edge.
3. **Back up first**: read the whole flash of the tag and keep the file (it contains the individual data of your tag, do not publish it).
4. Choose the `.bin` file and write the firmware. Do **not** press "Erase All Flash" and never mass-erase the chip: the MAC address and calibration data are stored in the same flash.
5. The LED blinks green (idle) or blue (a phone is connected) every 10 seconds. Open the control page and connect.

OTA over Bluetooth (upstream feature) must not be used with this firmware: it writes at 0x20000, where the slides are.

## First checks on a new tag

1. The page connects and shows the firmware version, the battery voltage and "0 slides". Compare the voltage with a multimeter; the scale has not been verified.
2. Upload one flat picture and press "show" (the refresh takes about 25 s). Colours and orientation must match the preview.
3. Upload two or three more, set the interval to 1 minute, disconnect: the slides must change one after another.
4. Remove and re-insert the battery: the slides and the settings must still be there.
