#!/usr/bin/env python3

import sys
import argparse
import os

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('text', nargs='*', type=str)
    args = parser.parse_args()

    text = args.text if isinstance(args.text, list) else [args.text]

    output = ''
    for t in text:
        t = os.path.basename(t)

        if not output:
            output += t
        else:
            output += ' ' + t

    output += '\n'

    sys.stdout.write(output)

if __name__ == '__main__':
    sys.exit(main())
