#!/usr/bin/env python3

from storer import Storer
import sys

s = Storer()

if s.get_value() != 0:
    print('Initial value incorrect.')
    sys.exit(1)

s.set_value(42)

if s.get_value() != 42:
    print('Setting value failed.')
    sys.exit(1)

try:
    s.set_value('not a number')
    print('Using wrong argument type did not fail.')
    sys.exit(1)
except TypeError:
    pass
