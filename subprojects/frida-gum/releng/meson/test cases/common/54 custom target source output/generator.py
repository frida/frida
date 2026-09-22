#!/usr/bin/env python3

import sys, os

if len(sys.argv) != 2:
    print(sys.argv[0], '<output dir>')

odir = sys.argv[1]

with open(os.path.join(odir, 'mylib.h'), 'w') as f:
    f.write('int func(void);\n')
with open(os.path.join(odir, 'mylib.c'), 'w') as f:
    f.write('''int func(void) {
    return 0;
}
''')
