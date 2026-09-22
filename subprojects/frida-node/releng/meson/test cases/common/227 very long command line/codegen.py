#!/usr/bin/env python3

import sys
from pathlib import Path

Path(sys.argv[2]).write_text(
    'int func{n}(void) {{ return {n}; }}'.format(n=sys.argv[1]))
