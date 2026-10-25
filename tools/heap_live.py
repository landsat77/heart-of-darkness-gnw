#!/usr/bin/env python3
"""Live heap blocks at the end of a PC run, by caller.

HD_HEAPTRACE=1 ./HeartOfDarkness_host ... 2> trace.txt
tools/heap_live.py HeartOfDarkness_host trace.txt
"""
import collections
import subprocess
import sys

exe, trace = sys.argv[1], sys.argv[2]
live = {}
for line in open(trace, errors="replace"):
    f = line.split()
    if len(f) == 4 and f[0] == "A":
        live[f[3]] = (int(f[1]), f[2])
    elif len(f) == 2 and f[0] == "F":
        live.pop(f[1], None)
by = collections.defaultdict(lambda: [0, 0])
for size, caller in live.values():
    by[caller][0] += size
    by[caller][1] += 1
callers = sorted(by)
names = subprocess.run(["addr2line", "-f", "-C", "-s", "-e", exe] + callers, capture_output=True, text=True).stdout.splitlines()
rows = []
for i, c in enumerate(callers):
    rows.append((by[c][0], by[c][1], names[2 * i][:60], names[2 * i + 1]))
rows.sort(reverse=True)
total = sum(r[0] for r in rows)
for r in rows[:40]:
    print(f"{r[0]:8} {r[1]:5}  {r[2]}  {r[3]}")
print(f"total {total} bytes in {sum(r[1] for r in rows)} blocks")
