#!/usr/bin/env python3

import os
import shlex
import subprocess


if 'MESONINTROSPECT' not in os.environ:
    raise RuntimeError('MESONINTROSPECT not found')
if 'MESON_BUILD_ROOT' not in os.environ:
    raise RuntimeError('MESON_BUILD_ROOT not found')

mesonintrospect = os.environ['MESONINTROSPECT']
introspect_arr = shlex.split(mesonintrospect)

buildroot = os.environ['MESON_BUILD_ROOT']

subprocess.check_output([*introspect_arr, '--all', buildroot])
