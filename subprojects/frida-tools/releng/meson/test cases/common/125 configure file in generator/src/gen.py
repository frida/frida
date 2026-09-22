#!/usr/bin/env python3

import sys

ifile = sys.argv[1]
ofile = sys.argv[2]

with open(ifile) as f:
    resval = f.readline().strip()

templ = '#define RESULT (%s)\n'
with open(ofile, 'w') as f:
    f.write(templ % (resval, ))
