#!/usr/bin/env python3
import sys

with open(sys.argv[1], 'w') as f:
    print('EXPORTS', file=f)
    print('        somedllfunc', file=f)
