#!/usr/bin/python3

import sys

while True:
    b = sys.stdin.buffer.read(1024)
    sys.stdout.buffer.write(b)

    if len(b) < 1024: break

