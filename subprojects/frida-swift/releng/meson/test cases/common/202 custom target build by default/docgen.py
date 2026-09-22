#!/usr/bin/env python3

import os
import sys

out = sys.argv[1]

os.mkdir(out)

for name in ('a', 'b', 'c'):
    with open(os.path.join(out, name + '.txt'), 'w') as f:
        f.write(name)
