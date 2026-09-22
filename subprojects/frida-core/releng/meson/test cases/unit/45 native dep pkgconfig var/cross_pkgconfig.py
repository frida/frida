#!/usr/bin/env python3

import os
import sys
import subprocess

environ = os.environ.copy()
environ['PKG_CONFIG_LIBDIR'] = os.path.join(
    os.path.dirname(os.path.realpath(__file__)), 'cross_pkgconfig')

sys.exit(
    subprocess.run(['pkg-config'] + sys.argv[1:], env=environ).returncode)
