#!/usr/bin/env python3

import os, sys, shutil

ifile = sys.argv[1]
ofile = sys.argv[2]

try:
    os.unlink(ofile)
except FileNotFoundError:
    pass

shutil.copy(ifile, ofile)
