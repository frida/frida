#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright © 2023 Intel Corporation

from __future__ import annotations
import argparse
import os
import typing as T

if T.TYPE_CHECKING:
    class Arguments(T.Protocol):
        checks_off: str
        checks_on: str


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('checks_off')
    parser.add_argument('checks_on')
    args: Arguments = parser.parse_args()

    off = os.stat(args.checks_off).st_size
    on = os.stat(args.checks_on).st_size

    assert on > off, f'Expected binary built with overflow-checks to be bigger, but it was smaller. with: "{on}"B, without: "{off}"B'


if __name__ == "__main__":
    main()
