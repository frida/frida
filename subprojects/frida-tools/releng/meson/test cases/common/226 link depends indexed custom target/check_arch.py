#!/usr/bin/env python3

import re
import sys
import shutil
import subprocess

exepath = sys.argv[1]
want_arch = sys.argv[2]
dummy_output = sys.argv[3]

with open(dummy_output, 'w') as f:
    f.write('')

if not shutil.which('dumpbin'):
    print('dumpbin not found, skipping')
    sys.exit(0)

out = subprocess.check_output(['dumpbin', '/HEADERS', exepath],
                              universal_newlines=True)
for line in out.split('\n'):
    m = re.match(r'.* machine \(([A-Za-z0-9]+)\)$', line)
    if m:
        arch = m.groups()[0].lower()

if arch == 'arm64':
    arch = 'aarch64'
elif arch == 'x64':
    arch = 'x86_64'

if arch != want_arch:
    raise RuntimeError(f'Wanted arch {want_arch} but exe uses {arch}')
