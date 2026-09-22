#!/usr/bin/env python3

from pathlib import Path
import sys

if __name__ == '__main__':
    Path(sys.argv[1]).write_text('Hello World\n')
    raise SystemExit(0)
