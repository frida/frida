#!/usr/bin/env python3

import sys, os
from pathlib import Path

if len(sys.argv) != 2:
    print("Wrong amount of parameters.")

build_dir = Path(os.environ['MESON_BUILD_ROOT'])
subdir = Path(os.environ['MESON_SUBDIR'])
outputf = Path(sys.argv[1])

with outputf.open('w') as ofile:
    ofile.write("#define ZERO_RESULT 0\n")
