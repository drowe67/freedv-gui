#!/usr/bin/python3

import sys

# Read entire "decoded" output into memory
with open("/home/parallels/freedv-gui/rx_radae_demo.s16", "rb") as f:
    entire_file = f.read()

ctr = 0
while True:
    b = sys.stdin.buffer.read(160)
    if len(b) < 160: break

    sys.stderr.write("read block\n")

    if (ctr + 160) > len(entire_file):
        sys.stdout.buffer.write(entire_file[ctr:])
        ctr = 0
    else:
        sys.stdout.buffer.write(entire_file[ctr:ctr + 160])
        ctr = ctr + 160
