#!/usr/bin/env python3

import sys, os

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print(sys.argv[0], 'n output objects...')
        sys.exit(1)
    if len(sys.argv) != int(sys.argv[1]) + 3:
        print(f'expected {sys.argv[1]} objects, got {len(sys.argv) - 3}')
        sys.exit(1)
    for i in sys.argv[3:]:
        print('testing', i)
        if not os.path.exists(i):
            sys.exit(1)
    with open(sys.argv[2], 'wb') as out:
        pass
