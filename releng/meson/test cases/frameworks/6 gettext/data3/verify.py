#!/usr/bin/env python3

import os
import sys

assert len(sys.argv) == 3

fname = sys.argv[1]
check_str = sys.argv[2]

assert os.path.isfile(fname)
with open(fname, 'r', encoding='utf-8') as f:
    assert check_str in f.read()
