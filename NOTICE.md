# Notice and provenance

badge154 is not affiliated with, or endorsed by, Hanshow, Telink, OpenEPaperLink or Solum. The names are used only to say
which hardware the project works with.

## What is in this repository

Everything here was written for this project and is under the MIT licence (file `LICENSE`):

- `docs/index.html`: the control page,
- `firmware/src/`: the slide storage / protocol code (`slides.c`) and the panel driver (`epd_bwry_154.c`),
- `firmware/apply.py`: the script that patches the upstream firmware,
- `tests/`, `docs/HARDWARE.md`.

## What is not in this repository

The firmware is built by applying `firmware/apply.py` to a pinned checkout of
[atc1441/ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper) (MIT licence, commit
`3778d296f418c30b05310d86dafa4e3404071cb4`). That checkout is not copied here. The compiled firmware file contains the
upstream code and the libraries it ships with, each under its own licence:

- Telink Semiconductor SDK (headers and precompiled BLE library `liblt_8258.a`), copyright Telink Semiconductor (Shanghai) Co., Ltd.,
- BitBank Software (OneBitDisplay, TIFF G4 decoder), Apache License 2.0,
- FreeRTOS parts, Amazon.com, Inc. (MIT),
- micro-ecc, Kenneth MacKay (BSD 2-clause), Xiaomi mijia BLE code (Apache License 2.0).

If you redistribute a compiled firmware file, keep those notices with it.

## Where the hardware information came from

The pin assignment, the initialisation command sequence of the display controller and the timing of the panel driver were
read from the firmware **ATC_BLE_OEPL** published by atc1441 (https://atc1441.github.io/), preset "38: 200 HS BWRY JD",
by disassembling the published binary, and were then checked on a real tag (see `docs/HARDWARE.md` for what was and was not
confirmed). The Gadgetbridge documentation describes ATC_BLE_OEPL as a fork of OpenEPaperLink; I did not find a licence statement for the
binary. The register values and pin numbers are facts about the hardware; no source code of that firmware was copied.
The driver code itself was written from scratch.

## Related project

[badge27](https://github.com/steanlab/badge27) by the same author: a similar slideshow for a different tag (Solum 2.7").
The control page of this project started from the page of badge27.
