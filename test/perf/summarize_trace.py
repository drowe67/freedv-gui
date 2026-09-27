#!/usr/bin/env python3
"""Summarizes an exported Time Profiler table (xctrace export ... time-profile) as JSON.

Reports CPU time per thread and main-thread time in the rendering paths that matter
for PlotScalar: CoreAnimation commits, in-process backing store rasterization, color
conversion and image drawing.

Usage: summarize_trace.py export.xml
"""
import json
import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter

MAIN_THREAD_FRAMES = {
    "ca_commit_ms": "CA::Transaction::commit()",
    "backing_store_update_ms": "CABackingStoreUpdate_",
    "color_convert_ms": "CGColorTransformConvertUsingCMSConverter",
    "draw_image_ms": "ripc_DrawImage",
    "scaled_blit_ms": "argb32_image_mark_argb32",
    "on_paint_ms": "PlotPanel::OnPaint(wxPaintEvent&)",
    "plot_scalar_draw_ms": "PlotScalar::draw(wxGraphicsContext*, bool)",
}
THREADS = {"main_thread_ms": "Main Thread", "tx_thread_ms": "FDV txThread", "rx_thread_ms": "FDV rxThread"}

ids = {}

def register(el):
    for sub in el.iter():
        if sub.get("id") is not None:
            ids[sub.get("id")] = sub

def resolve(el):
    if el is None:
        return None
    ref = el.get("ref")
    return ids[ref] if ref is not None else el

threads = Counter()
frames = Counter()
total = 0
first = last = None
for _, row in ET.iterparse(sys.argv[1], events=("end",)):
    if row.tag != "row":
        continue
    register(row)
    t = int(resolve(row.find("sample-time")).text)
    first = t if first is None else min(first, t)
    last = t if last is None else max(last, t)
    weight = int(resolve(row.find("weight")).text)
    total += weight
    thread = re.sub(r" \(0x.*", "", resolve(row.find("thread")).get("fmt", ""))
    threads[thread] += weight
    if thread != "Main Thread":
        continue
    backtrace = resolve(row.find("tagged-backtrace"))
    if backtrace is None:
        continue
    names = {resolve(f).get("name", "") for f in backtrace.findall("frame")}
    for key, frame in MAIN_THREAD_FRAMES.items():
        if frame in names:
            frames[key] += weight

result = {"span_s": round(((last or 0) - (first or 0)) / 1e9, 1), "total_cpu_ms": round(total / 1e6)}
for key, name in THREADS.items():
    result[key] = round(threads[name] / 1e6)
for key in MAIN_THREAD_FRAMES:
    result[key] = round(frames[key] / 1e6)
print(json.dumps(result))
