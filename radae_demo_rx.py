#!/usr/bin/python3

import sys

# Read entire "decoded" output into memory
with open("/home/parallels/freedv-gui/rx_radae_demo.s16", "rb") as f:
    entire_file = f.read()

ctr = 0
while True:
    b = sys.stdin.buffer.read(1024)
    if len(b) < 1024: break

    if (ctr + 1024) > len(entire_file):
        sys.stdout.buffer.write(entire_file[ctr:])
        ctr = 0
    else:
        sys.stdout.buffer.write(entire_file[ctr:ctr + 1024])
        ctr = ctr + 1024    
