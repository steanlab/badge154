#!/usr/bin/env bash
# Fails if the RAM of the firmware reaches into the stack.
# The SRAM of the TLSR8258 family is 64 KB (0x840000 - 0x84FFFF); the stack starts at 0x850000 and grows down.
# Usage: firmware/check_ram.sh <path to ATC_Paper.elf> [path to tc32-elf-nm]
set -euo pipefail
elf="$1"
nm="${2:-$(dirname "$elf")/../tc32_linux/bin/tc32-elf-nm}"
end=$("$nm" "$elf" | awk '$3=="_end_bss_"{print $1}')
limit=$((0x84F000))       # leaves at least 4 KB for the stack
val=$((16#$end))
echo "end of .bss: 0x$end   stack space left: $((0x850000 - val)) bytes (minimum 4096)"
if [ "$val" -gt "$limit" ]; then echo "ERROR: RAM overflow"; exit 1; fi
