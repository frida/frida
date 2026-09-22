#!/usr/bin/env python3

import sys

ifile = open(sys.argv[1])
ofile = open(sys.argv[2], 'w')

ofile.write(ifile.read())
