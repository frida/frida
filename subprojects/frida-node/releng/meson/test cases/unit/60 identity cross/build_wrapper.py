#!/usr/bin/env python3

import subprocess, sys, platform

# Meson does not yet support Studio cc on Solaris, only gcc or clang
if platform.system() == 'SunOS':
    cc = 'gcc'
else:
    cc = 'cc'

subprocess.call([cc, "-DEXTERNAL_BUILD"] + sys.argv[1:])
