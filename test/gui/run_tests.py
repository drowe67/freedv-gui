#!/usr/bin/env python3
"""Runs GUI test scenarios one at a time, each in a process of its own, and
says which passed, were skipped (exit code 77) and failed, as CTest does on
Linux and macOS. For Windows, where FreeDV is cross-compiled elsewhere and
there's no build folder to run CTest in.

    python run_tests.py [--retries N] [--exclude script:scenario]... [--logs DIR]
                        [script | script:scenario]...

With nothing named, every scenario of every script (test_<script>.py) is run.
A scenario that fails is run again, up to --retries more times. Each one's
output goes to <logs>/<script>_<scenario>.txt (and that of a try that failed
before another was made to <script>_<scenario>.try<N>.txt). The output of
every failed try is printed at the end, so that one that passed on a retry
can still be looked into. The exit code is 1 if any failed every time.

FreeDV is $FREEDV_BINARY (see freedvtest.default_binary()).
"""

import argparse
import glob
import importlib
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SKIPPED = 77


def scenarios(script):
    sys.path.insert(0, HERE)
    return list(importlib.import_module("test_" + script).SCENARIOS)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--retries", type=int, default=0)
    parser.add_argument("--exclude", action="append", default=[])
    parser.add_argument("--logs", default="gui_test_logs")
    parser.add_argument("--timeout", type=int, default=900, help="seconds each scenario may take")
    parser.add_argument("names", nargs="*")
    args = parser.parse_args()

    scripts = sorted(os.path.basename(path)[len("test_"):-len(".py")]
                     for path in glob.glob(os.path.join(HERE, "test_*.py")))
    chosen = []
    for name in args.names or scripts:
        script, _, scenario = name.partition(":")
        chosen += [(script, scenario)] if scenario else [(script, each) for each in scenarios(script)]
    chosen = [pair for pair in chosen if "%s:%s" % pair not in args.exclude]

    os.makedirs(args.logs, exist_ok=True)
    results = {}
    retried = []
    for script, scenario in chosen:
        log = os.path.join(args.logs, "%s_%s.txt" % (script, scenario))
        for attempt in range(args.retries + 1):
            if attempt:
                # Keep the failed try's output from being overwritten.
                kept = os.path.join(args.logs, "%s_%s.try%d.txt" % (script, scenario, attempt))
                os.replace(log, kept)
                retried.append(((script, scenario), attempt, kept))
            start = time.monotonic()
            with open(log, "wb") as f:
                try:
                    code = subprocess.call([sys.executable, os.path.join(HERE, "test_%s.py" % script), scenario],
                                           stdout=f, stderr=subprocess.STDOUT, timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    code = "timeout"
            result = "passed" if code == 0 else "skipped" if code == SKIPPED else "failed (%s)" % code
            print("%-14s %-36s %-14s %4ds%s" % (script, scenario, result, time.monotonic() - start,
                                                " (try %d)" % (attempt + 1) if attempt else ""), flush=True)
            if not result.startswith("failed"):
                break
        results[(script, scenario)] = (result, log)

    for (script, scenario), attempt, log in retried:
        print("\n===== %s:%s (try %d) =====" % (script, scenario, attempt))
        with open(log, "r", errors="replace") as f:
            sys.stdout.write(f.read()[-20000:])
    failed = [(pair, log) for pair, (result, log) in results.items() if result.startswith("failed")]
    for (script, scenario), log in failed:
        print("\n===== %s:%s =====" % (script, scenario))
        with open(log, "r", errors="replace") as f:
            sys.stdout.write(f.read()[-20000:])
    counts = {}
    for result, _ in results.values():
        counts[result.split(" ")[0]] = counts.get(result.split(" ")[0], 0) + 1
    print("\n%d scenarios: %s" % (len(results), ", ".join("%d %s" % (n, kind) for kind, n in sorted(counts.items()))))
    if failed:
        print("Failed: " + ", ".join("%s:%s" % pair for pair, _ in failed))
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
