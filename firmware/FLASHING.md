# Flashing the firmware

The firmware file `badge154-firmware-v1.0.0.bin` (with `SHA256SUMS.txt`) is in this folder and in the [Releases](https://github.com/steanlab/badge154/releases). The source code of the firmware is not published; the file replaces the original firmware of the tag, which is updated only by wire (there is no update over Bluetooth).
It fits the Nebular Pro 154Q-N, Plus 154Q-N and 154R-N.

## Flashing

Wired, with a USB-UART adapter and atc1441's web flasher. The firmware is the same for the Nebular Pro 154Q-N, Plus 154Q-N and 154R-N (tested on all three); only the positions of the pads differ ([`docs/HARDWARE.md`](../docs/HARDWARE.md#hardware-variants)).

1. Wire the tag: **GND**, **3.3 V** (only if the battery is out) and **SWS** to **TXD** of the adapter, **NRST** to **DTR** of the adapter.
2. Open https://atc1441.github.io/ATC_BLE_OEPL_Image_Upload.html in Chrome or Edge and pick the USB-COM port.
3. **Back up first.** Read the whole flash and keep the file somewhere private. It holds the individual data of your tag (MAC, calibration) and the stock firmware: do not publish it and do not flash it into another tag. A dump is taken with [hanshow-rp2040-programmer](https://github.com/gerard780/hanshow-rp2040-programmer) (a Waveshare RP2040-Zero as an SWS bridge). It was used here only for reading, and it reads the stock flash of all three variants. The author calls the current version experimental: take the dump twice and compare the two files. Wire it as its README says: never put 5 V on the tag's VCC, and take the battery out while the tag is powered from the programmer's 3.3 V. Stock dumps are not distributed here.
4. Load `badge154-firmware-v1.0.0.bin` (from the [Releases](https://github.com/steanlab/badge154/releases) or from this folder; check it against `SHA256SUMS.txt`) and press **Write Firmware**. Do **not** press "Erase All Flash" and never mass-erase the chip.
5. Disconnect the adapter and restart the tag: Plus: take the coin cell out for 5 to 10 s and put it back; Pro and 154R-N (soldered cell): touch NRST to GND for a moment. The LED blinks blue once and the start-up screen is drawn. Open the control page and connect.

## First checks on a new tag

1. The page connects and shows the firmware version, the battery voltage and "0 slides".
2. Upload one flat picture, press **show** (about 25 s): the colours and orientation must match the preview.
3. Upload more, set the interval to 1 minute, disconnect: the slides change one after another. An NFC tap shows the next one.
4. Restart the tag (as in step 5 above): slides, settings and name are still there.
