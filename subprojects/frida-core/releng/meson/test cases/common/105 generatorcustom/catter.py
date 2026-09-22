#!/usr/bin/env python3

import sys

output = sys.argv[-1]
inputs = sys.argv[1:-1]

with open(output, 'w') as ofile:
    ofile.write('#pragma once\n')
    for i in inputs:
        with open(i) as ifile:
            content = ifile.read()
        ofile.write(content)
        ofile.write('\n')
