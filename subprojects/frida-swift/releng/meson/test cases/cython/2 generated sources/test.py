#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0

import argparse
import importlib

parser = argparse.ArgumentParser()
parser.add_argument('mod')
args = parser.parse_args()

mod = importlib.import_module(args.mod)

assert mod.func() == 'Hello, World!'
