#!/usr/bin/env python3
import os
import sys

ENV_VAR_VALUE = os.environ.get('ENV_VAR_VALUE')
assert ENV_VAR_VALUE is not None

with open(sys.argv[1], 'r') as infile, \
     open(sys.argv[2], 'w') as outfile:
    
    outfile.write(infile.read().replace('ENV_VAR_VALUE', ENV_VAR_VALUE))
