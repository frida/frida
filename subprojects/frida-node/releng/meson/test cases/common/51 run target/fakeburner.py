#!/usr/bin/env python3


import sys

plain_arg = sys.argv[1]
_, filename, _ = plain_arg.split(':')
try:
    with open(filename, 'rb') as f:
        content = f.read()
except FileNotFoundError:
    print('Could not open file. Missing dependency?')
    sys.exit(1)
print('File opened, pretending to send it somewhere.')
print(len(content), 'bytes uploaded')
