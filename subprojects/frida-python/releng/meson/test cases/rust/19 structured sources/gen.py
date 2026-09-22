#!/usr/bin/env python3

import argparse
import textwrap


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('output')
    args = parser.parse_args()

    with open(args.output, 'w') as f:
        f.write(textwrap.dedent('''\
            pub fn bar() -> () {
                println!("Hello, World!");
            }'''))


if __name__ == "__main__":
    main()
