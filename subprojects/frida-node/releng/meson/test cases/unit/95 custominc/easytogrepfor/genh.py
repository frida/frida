#!/usr/bin/env python3

import sys

f = open(sys.argv[1], 'w')
f.write('#define RETURN_VALUE 0')
f.close()
