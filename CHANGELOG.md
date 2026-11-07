# Changelog

## v1.0.0 (2026-11)

First public release.

- Slideshow in the tag's own flash: 98 blocks of 4 KB, every slide checked by a CRC, upload over Web Bluetooth from the control page (installable app, works offline).
- Automatic change every N minutes, in order or at random; NFC tap shows the next slide.
- Tag name (shown in the Bluetooth list as `badge154-<name>`), hardware report on the page or on the tag screen.
- Watchdog (8 s), low-battery mode, nine radio power levels.
- Control page v1.21: dithering methods (Floyd-Steinberg, Atkinson, Stucki and others, blue noise, ordered), colours measured on the real panel, one-click presets for photos and for logos / text, small softened preview.
- Tested on Nebular Pro 154Q-N, Plus 154Q-N and 154R-N.
- Known limits: upload speed about 0.6 KB/s; low-battery thresholds, radio power levels and battery voltage scale not verified on the tag; current consumption not measured.
