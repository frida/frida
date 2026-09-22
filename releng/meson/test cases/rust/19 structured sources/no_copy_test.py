#!/usr/bin/env python3

import argparse
import os


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('builddir')
    args = parser.parse_args()

    for _, _, files in os.walk(args.builddir):
        if 'main-unique.rs' in files:
            exit(1)


if __name__ == "__main__":
    main()
