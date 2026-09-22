#!/usr/bin/env python3

import os
import sys

for fh in os.listdir('.'):
    if os.path.isfile(fh):
        if fh.endswith('.c'):
            sys.stdout.write(fh + '\0')
