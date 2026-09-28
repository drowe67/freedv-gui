#!/usr/bin/env python3
"""For a capture (test.wav) and FreeDV's own TX output (tx_out.wav), both converted to raw
16-bit mono 48 kHz: lists the silent gaps inside the captured transmission and compares
active durations, to tell inserted silence (capture longer) from replaced signal.
Usage: gaps_vs_ref.py <capture.raw> <tx_out.raw> <label>"""
import sys
import numpy as np
sr = 48000
cap = np.fromfile(sys.argv[1], dtype=np.int16).astype(float)
ref = np.fromfile(sys.argv[2], dtype=np.int16).astype(float)
def active(x):
    blk = sr // 100
    rms = np.array([np.sqrt(np.mean(x[i:i+blk]**2)) for i in range(0, len(x)-blk, blk)])
    a = np.where(rms > 0.3 * rms.max())[0]
    return a[0]*blk, (a[-1]+1)*blk
cs, ce = active(cap); rs, re_ = active(ref)
y = cap[cs:ce]
zero = np.abs(y) <= 2
edges = np.flatnonzero(np.diff(np.concatenate([[0], zero.astype(int), [0]])))
st, en = edges[::2], edges[1::2]
keep = (en - st) >= 256
gaps = [(s/sr, (e-s)) for s, e in zip(st[keep], en[keep])]
tot = sum(g[1] for g in gaps)
print(f"{sys.argv[3]}: capture active {(ce-cs)/sr:.3f} s, FreeDV TX output active {(re_-rs)/sr:.3f} s, "
      f"difference {(ce-cs)-(re_-rs):+d} samples; gaps total {tot} samples: " +
      ", ".join(f"{t:.2f}s x{n}" for t, n in gaps))
