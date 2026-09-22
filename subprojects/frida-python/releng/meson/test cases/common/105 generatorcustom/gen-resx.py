#!/usr/bin/env python3

import sys

ofile = sys.argv[1]
num = sys.argv[2]

with open(ofile, 'w') as f:
    f.write(f'res{num}\n')
