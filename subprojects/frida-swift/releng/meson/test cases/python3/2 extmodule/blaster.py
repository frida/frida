#!/usr/bin/env python3

import tachyon
import sys

result = tachyon.phaserize('shoot')

if not isinstance(result, int):
    print('Returned result not an integer.')
    sys.exit(1)

if result != 1:
    print(f'Returned result {result} is not 1.')
    sys.exit(1)
