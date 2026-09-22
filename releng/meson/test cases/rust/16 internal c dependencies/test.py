#!/usr/bin/env python3

import argparse
import subprocess
import sys


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('command')
    parser.add_argument('expected')
    args = parser.parse_args()

    out = subprocess.run(args.command, stdout=subprocess.PIPE)
    actual = out.stdout.decode().strip()

    if args.expected != actual:
        print('expected:', args.expected, file=sys.stderr)
        print('actual:  ', actual, file=sys.stderr)
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
