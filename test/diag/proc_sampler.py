#!/usr/bin/env python3
"""Samples which processes used the most CPU, every INTERVAL seconds, until SIGTERM/SIGINT.

    proc_sampler.py <output file> [interval seconds, default 0.2]

Each line: host time (CLOCK_UPTIME_RAW, i.e. mach_absolute_time(), in seconds -- the same
clock as FreeDV's TIMINGDIAG lines), wall time (Unix seconds), the interval actually
covered, total CPU used by all processes in it, and the top processes by CPU used in it
(from differences in ps's cumulative CPU time). A long "covered" interval means the
sampler itself didn't get to run, e.g. during a machine-wide pause.
"""
import signal
import subprocess
import sys
import time

out = open(sys.argv[1], "w", buffering=1)
interval = float(sys.argv[2]) if len(sys.argv) > 2 else 0.2
stop = False


def on_signal(signum, frame):
    global stop
    stop = True


signal.signal(signal.SIGTERM, on_signal)
signal.signal(signal.SIGINT, on_signal)


def cpu_seconds(text):
    # ps's "time" column: [[dd-]hh:]mm:ss.ss
    days = 0
    if "-" in text:
        d, text = text.split("-", 1)
        days = int(d)
    parts = [float(p) for p in text.split(":")]
    secs = 0.0
    for p in parts:
        secs = secs * 60 + p
    return days * 86400 + secs


def snapshot():
    ps = subprocess.run(["ps", "-A", "-o", "pid=,time=,comm="], capture_output=True, text=True).stdout
    procs = {}
    for line in ps.splitlines():
        fields = line.split(None, 2)
        if len(fields) < 3:
            continue
        try:
            procs[int(fields[0])] = (cpu_seconds(fields[1]), fields[2].rsplit("/", 1)[-1])
        except ValueError:
            continue
    return procs


prev = snapshot()
prev_host = time.clock_gettime(time.CLOCK_UPTIME_RAW)
out.write(f"# proc_sampler started: host {prev_host:.3f} wall {time.time():.3f} interval {interval}\n")
while not stop:
    time.sleep(interval)
    host = time.clock_gettime(time.CLOCK_UPTIME_RAW)
    wall = time.time()
    cur = snapshot()
    deltas = []
    for pid, (cpu, name) in cur.items():
        if pid in prev:
            d = cpu - prev[pid][0]
            if d > 0:
                deltas.append((d, name, pid))
    deltas.sort(reverse=True)
    total = sum(d for d, _, _ in deltas)
    top = " ".join(f"{name}({pid})={1000 * d:.0f}ms" for d, name, pid in deltas[:6])
    out.write(f"host {host:.3f} wall {wall:.3f} covered {1000 * (host - prev_host):.0f}ms total {1000 * total:.0f}ms | {top}\n")
    prev, prev_host = cur, host
out.write(f"# proc_sampler stopped: host {time.clock_gettime(time.CLOCK_UPTIME_RAW):.3f}\n")
