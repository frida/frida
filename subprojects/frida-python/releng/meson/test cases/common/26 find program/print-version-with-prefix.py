#!/usr/bin/env python3

import sys

if len(sys.argv) != 2 or sys.argv[1] != '--version':
    exit(1)

print('Version: 1.0')
