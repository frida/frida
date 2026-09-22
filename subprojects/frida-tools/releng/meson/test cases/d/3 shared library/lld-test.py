#!/usr/bin/env python3

import argparse
import subprocess

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('ldd')
    parser.add_argument('bin')
    args = parser.parse_args()

    p, o, _ = subprocess.run([args.ldd, args.bin], stdout=subprocess.PIPE)
    assert p == 0
    o = o.decode()
    assert 'libstuff.so =>' in o, 'libstuff so not in linker path.'
    assert 'libstuff.so => not found' not in o, 'libstuff.so not found correctly'


if __name__ == '__main__':
    main()
