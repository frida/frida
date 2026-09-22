#!/usr/bin/env python3

import sys

with open(sys.argv[1], 'rb') as infile, open(sys.argv[2], 'wb') as outfile:
    outfile.write(infile.read())
