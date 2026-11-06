#!/usr/bin/env bash
# Host tests: the real slides.c (storage, protocol, LZ4 decoder, timer) against a flash emulator,
# and the JS codec of the page against the C code.  Needs gcc, node and python3.
set -euo pipefail
cd "$(dirname "$0")"
gcc -O1 -shared -fPIC -DSLIDES_HOST -I. -I../firmware/src -o libslides.so ../firmware/src/slides.c host_impl.c -Wall
python3 -I t.py
node codec_test.js
python3 -I t2.py
echo "all tests passed"
