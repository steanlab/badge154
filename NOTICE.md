# Notice and provenance

badge154 is not affiliated with, or endorsed by, Hanshow, Telink, OpenEPaperLink or Solum. The names are used only to say
which hardware the project works with.

## What is in this repository

The control page (`docs/index.html` and the installable-app files) and the documents were written for this project and are under the MIT licence (file `LICENSE`).
The repository also holds the compiled firmware file. Its source code is **not** published. The firmware was written for this project, except for the files of the upstream project it modifies
(`epd.c`, `epd.h`, `epd_ble_service.c`, `nfc.c`, `project.mk`; MIT licence, the text is in `firmware/ATC_TLSR_Paper-LICENSE.txt`).

## What the firmware file contains

The firmware is built by applying the project's own files to a pinned checkout of
[atc1441/ATC_TLSR_Paper](https://github.com/atc1441/ATC_TLSR_Paper) (MIT licence, commit
`3778d296f418c30b05310d86dafa4e3404071cb4`). The compiled firmware file contains the
upstream code and the libraries it ships with, each under its own licence:

- Telink Semiconductor SDK (headers and precompiled BLE library `liblt_8258.a`), copyright Telink Semiconductor (Shanghai) Co., Ltd.,
- BitBank Software (OneBitDisplay, TIFF G4 decoder), Apache License 2.0,
- FreeRTOS parts, Amazon.com, Inc. (MIT),
- micro-ecc, Kenneth MacKay (BSD 2-clause), Xiaomi mijia BLE code (Apache License 2.0).

If you redistribute a compiled firmware file, keep those notices with it. The compiled firmware is published in the Releases and in `firmware/`; it contains the parts listed above.

Note on the Telink SDK: the file headers of the SDK that the upstream repository ships say that the information is available under a commercial licence agreement or "free, non-transferable use ... under Mutual Non-Disclosure Agreement", with no warranty.
The upstream project distributes the SDK sources and a compiled firmware openly, and so do other community projects for these chips, but the licence terms were not reviewed by a lawyer.
Whoever redistributes the binary does so on their own responsibility; if Telink or the upstream author objects, the file will be removed.

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
