#!/usr/bin/env python3
"""Feeds every instruction objdump finds in the given binaries to x64len_check.

Usage: check_x64len.py <objdump> <x64len_check binary> <file> [<file> ...]
Instructions objdump itself cannot decode ("(bad)") are skipped.
"""
import re, subprocess, sys

objdump, checker, files = sys.argv[1], sys.argv[2], sys.argv[3:]
row = re.compile(r'^\s*[0-9a-f]+:\t((?:[0-9a-f]{2} )+)\s*(?:\t(.*))?$')
rc = 0
for path in files:
    out = subprocess.run([objdump, "-d", "-w", path], capture_output=True, text=True).stdout.splitlines()
    insns = []
    for l in out:
        m = row.match(l)
        if m and "(bad)" not in (m.group(2) or ""):
            insns.append(m.group(1).split())
        elif not m and l.strip().endswith(":") and l.strip()[0] != ".":
            insns.append(None)          # function boundary: do not stitch across it
    feed = []
    for k, b in enumerate(insns):
        if b is None: continue
        tail = []
        j = k + 1
        while j < len(insns) and insns[j] is not None and len(tail) < 16:
            tail += insns[j]; j += 1
        feed.append("%d %s\n" % (len(b), "".join((b + tail)[:31])))
    r = subprocess.run([checker], input="".join(feed), capture_output=True, text=True)
    print("==", path); print(r.stdout.strip()); rc |= r.returncode
sys.exit(rc)
