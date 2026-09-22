#!/usr/bin/env python3

import sys

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(sys.argv[0], 'input_file')
        sys.exit(1)
    with open(sys.argv[1]) as f:
        ifile = f.read()
    if ifile != 'This is a text only input file.\n':
        print('Malformed input')
        sys.exit(1)
    print('This is a binary output file.')
