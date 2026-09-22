#!/usr/bin/env python3

import sys

with open(sys.argv[1]) as infile, open(sys.argv[2], 'w') as outfile:
    outfile.write(infile.read().format(icon=sys.argv[3]))
