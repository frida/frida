#!/usr/bin/env python3

import os
import sys

out = sys.argv[1]

try:
    os.mkdir(out)
except FileExistsError:
    pass

for name in ('a', 'b', 'c'):
    with open(os.path.join(out, name + '.html'), 'w') as f:
        f.write(name)
