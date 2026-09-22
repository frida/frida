#!/usr/bin/env python3

import sys, os

if len(sys.argv) != 3:
    print("You is fail.")
    sys.exit(1)

with open(sys.argv[1]) as f:
    val = f.read().strip()
outdir = sys.argv[2]

outhdr = os.path.join(outdir, 'source%s.h' % val)
outsrc = os.path.join(outdir, 'source%s.cpp' % val)

with open(outhdr, 'w') as f:
    f.write('int func%s();\n' % val)
with open(outsrc, 'w') as f:
    f.write('''int func%s() {
    return 0;
}
''' % val)
