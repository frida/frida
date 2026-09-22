#!/usr/bin/env python3

# NOTE: this file does not have the executable bit set. This tests that
# Meson can automatically parse shebang lines.

import sys

template = '#define RET_VAL %s\n'
with open(sys.argv[1]) as f:
    output = template % (f.readline().strip(), )
with open(sys.argv[2], 'w') as f:
    f.write(output)
