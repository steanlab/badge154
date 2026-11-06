#!/usr/bin/env bash
# Fails if the firmware image reaches into the slide area (starts at 0x14000, see firmware/src/slides.h).
# Usage: firmware/check_size.sh <path to ATC_Paper.bin>
set -euo pipefail
bin="$1"
size=$(stat -c %s "$bin")
limit=$((0x14000))
echo "firmware: $size bytes, limit $limit, room left: $((limit - size)) bytes"
if [ "$size" -gt "$limit" ]; then echo "ERROR: the firmware overlaps the slide area"; exit 1; fi
