#!/usr/bin/env python3

import sys
from pathlib import Path

files = [Path(f) for f in sys.argv[1:]]
names = [f.name for f in files]

assert names == ['check_inputs.txt', 'prog.c', 'prog.c', 'prog2.c', 'prog4.c', 'prog5.c']
for f in files[1:]:
    assert f.exists()

with files[0].open('w') as ofile:
    ofile.write("#define ZERO_RESULT 0\n")
