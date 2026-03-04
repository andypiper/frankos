#!/usr/bin/env python3
"""Generate digger.inf — text-only (name line, no icon data).
Icons are loaded from .ico files by file_assoc_scan()."""
import sys

name = sys.argv[1]
out_path = sys.argv[2]

with open(out_path, 'wb') as f:
    f.write(name.encode('ascii') + b'\n')
