#!/usr/bin/env python3

import sys, os

template = '''#pragma once

#define THE_NUMBER {}
#define THE_ARG1 {}
#define THE_ARG2 {}
'''

input_file = os.path.join(os.environ['MESON_SOURCE_ROOT'], 'raw.dat')
output_file = os.path.join(os.environ['MESON_BUILD_ROOT'], 'generated.h')

with open(input_file, encoding='utf-8') as f:
    data = f.readline().strip()
with open(output_file, 'w', encoding='utf-8') as f:
    f.write(template.format(data, sys.argv[1], sys.argv[2]))
