#!/usr/bin/env python3

import argparse


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('out')
    parser.add_argument('--mode', choices=['main', 'lib'], default='main')
    args = parser.parse_args()

    with open(args.out, 'w') as f:
        if args.mode == 'main':
            f.write('fn main() { println!("I prefer tarnish, actually.") }')
        elif args.mode == 'lib':
            f.write('pub fn libfun() { println!("I prefer tarnish, actually.") }')


if __name__ == "__main__":
    main()
