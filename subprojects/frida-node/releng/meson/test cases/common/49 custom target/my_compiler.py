#!/usr/bin/env python3

import os
import sys

assert os.path.exists(sys.argv[3])

args = sys.argv[:-1]

if __name__ == '__main__':
    assert os.environ['MY_COMPILER_ENV'] == 'value'
    if len(args) != 3 or not args[1].startswith('--input') or \
       not args[2].startswith('--output'):
        print(args[0], '--input=input_file --output=output_file')
        sys.exit(1)
    with open(args[1].split('=')[1]) as f:
        ifile = f.read()
    if ifile != 'This is a text only input file.\n':
        print('Malformed input')
        sys.exit(1)
    with open(args[2].split('=')[1], 'w') as ofile:
        ofile.write('This is a binary output file.\n')
