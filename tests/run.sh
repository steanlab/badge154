#!/usr/bin/env bash
# Host tests: the real slides.c (storage, protocol, LZ4 decoder, timer) against a flash emulator,
# and the JS codec of the page against the C code.  Needs gcc, node and python3.
set -euo pipefail
cd "$(dirname "$0")"
gcc -O1 -shared -fPIC -DSLIDES_HOST -I. -I../firmware/src -o libslides.so ../firmware/src/slides.c ../firmware/src/textscr.c host_impl.c -Wall
python3 -I t.py
node codec_test.js
python3 -I t2.py
python3 -I t3.py
# fuzz: hostile commands, hostile slide data, damaged flash; AddressSanitizer + UndefinedBehaviorSanitizer
gcc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -DSLIDES_HOST -I. -I../firmware/src \
    -o fuzz ../firmware/src/slides.c ../firmware/src/textscr.c host_impl.c fuzz.c -Wall
for seed in 1 2 3 4 5 6; do FUZZ_SEED=$seed FUZZ_ITERS=${FUZZ_ITERS:-1500} ./fuzz; done
echo "all tests passed"
