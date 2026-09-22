#!python3

import sys
from pathlib import Path

def main():
    if len(sys.argv) != 2:
        print(sys.argv)
        return 1
    if sys.argv[1] != 'gen.c':
        print(sys.argv)
        return 2
    Path('foo').touch()

    return 0

if __name__ == '__main__':
    sys.exit(main())
