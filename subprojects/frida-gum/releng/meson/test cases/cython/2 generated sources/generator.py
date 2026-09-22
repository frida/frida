#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

import argparse

parser = argparse.ArgumentParser()
parser.add_argument('input')
parser.add_argument('output')
args = parser.parse_args()

with open(args.input) as i, open(args.output, 'w') as o:
    o.write(i.read())
