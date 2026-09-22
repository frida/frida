#!/usr/bin/env python3

import os
import sys
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('target_dir',
                    help='the target dir')
parser.add_argument('stem',
                    help='the stem')
parser.add_argument('input',
                    help='the input file')

options = parser.parse_args(sys.argv[1:])

with open(options.input) as f:
    content = f.read()


output_c = os.path.join(options.target_dir, options.stem + ".tab.c")
with open(output_c, 'w') as f:
    f.write(content)


output_h = os.path.join(options.target_dir, options.stem + ".tab.h")
h_content = '''#pragma once

int myfun(void);
'''
with open(output_h, 'w') as f:
    f.write(h_content)
