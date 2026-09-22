#!/usr/bin/env python3

import os
import sys
import shlex

do_print = False

if len(sys.argv) > 1:
    do_print = bool(sys.argv[1])

if 'MESONINTROSPECT' not in os.environ:
    raise RuntimeError('MESONINTROSPECT not found')

mesonintrospect = os.environ['MESONINTROSPECT']

introspect_arr = shlex.split(mesonintrospect)

# print(mesonintrospect)
# print(introspect_arr)

some_executable = introspect_arr[0]

if not os.path.isfile(some_executable):
    raise RuntimeError(f'{mesonintrospect!r} does not exist')

if do_print:
    print(some_executable, end='')
