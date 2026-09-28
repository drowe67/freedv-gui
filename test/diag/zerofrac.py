#!/usr/bin/env python3
"""Measures how much of the TX signal is missing (zeroed) from a capture, per 1024-sample
device buffer. Usage: zerofrac.py <capture.raw> [label]   (16-bit mono 48 kHz)"""
import sys
import numpy as np
sr = 48000
x = np.fromfile(sys.argv[1], dtype=np.int16).astype(float)
label = sys.argv[2] if len(sys.argv) > 2 else ""
blk = sr // 10
rms = np.array([np.sqrt(np.mean(x[i:i+blk]**2)) for i in range(0, len(x)-blk, blk)])
act = np.where(rms > 0.3 * rms.max())[0]
y = x[act[0]*blk + sr//2:(act[-1]+1)*blk - sr//2]
zero = np.abs(y) <= 2
# runs of >= 8 consecutive near-zero samples inside the transmission
edges = np.flatnonzero(np.diff(np.concatenate([[0], zero.astype(int), [0]])))
starts, ends = edges[::2], edges[1::2]
lens = ends - starts
keep = lens >= 8
missing = lens[keep].sum() / len(y)
phase = starts[keep] % 1024
vals, counts = np.unique(lens[keep], return_counts=True)
top = sorted(zip(counts, vals), reverse=True)[:4]
print(f"{label}: {100*missing:.1f}% of the transmission is zeroed; gaps per second {keep.sum()/(len(y)/sr):.1f}; "
      f"gap lengths (samples:count) " + ", ".join(f"{v}:{c}" for c, v in top))
