#!/usr/bin/env python3

import os
import os.path
import sys


def main():
    print('Looking in:', os.getcwd())
    not_found = list()
    for f in sys.argv[1:]:
        if not os.path.exists(f):
            not_found.append(f)
    if not_found:
        print('Not found:', ', '.join(not_found))
        sys.exit(1)


if __name__ == '__main__':
    main()
