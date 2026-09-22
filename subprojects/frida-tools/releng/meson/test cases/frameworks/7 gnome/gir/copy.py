#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright © 2021 Intel Corporation

import argparse
import shutil

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('src')
    parser.add_argument('dest')
    args = parser.parse_args()

    shutil.copy(args.src, args.dest)


if __name__ == "__main__":
    main()
