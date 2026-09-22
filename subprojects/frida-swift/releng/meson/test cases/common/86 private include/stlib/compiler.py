#!/usr/bin/env python3

import sys, os

assert len(sys.argv) == 3

h_templ = '''#pragma once
unsigned int %s(void);
'''

c_templ = '''#include"%s.h"

unsigned int %s(void) {
  return 0;
}
'''

ifile = sys.argv[1]
outdir = sys.argv[2]

base = os.path.splitext(os.path.split(ifile)[-1])[0]

cfile = os.path.join(outdir, base + '.c')
hfile = os.path.join(outdir, base + '.h')

c_code = c_templ % (base, base)
h_code = h_templ % base

with open(cfile, 'w') as f:
    f.write(c_code)
with open(hfile, 'w') as f:
    f.write(h_code)
