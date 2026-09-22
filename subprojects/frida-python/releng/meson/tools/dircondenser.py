#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2018 The Meson development team

'''Renames test case directories using Git from this:

1 something
3 other
3 foo
3 bar

to this:

1 something
2 other
3 foo
4 bar

This directory must be run from source root as it touches run_unittests.py.
'''

import typing as T
import os
import sys
import subprocess

from glob import glob

def get_entries() -> T.List[T.Tuple[int, str]]:
    entries = []
    for e in glob('*'):
        if not os.path.isdir(e):
            raise SystemExit('Current directory must not contain any files.')
        (number, rest) = e.split(' ', 1)
        try:
            numstr = int(number)
        except ValueError:
            raise SystemExit(f'Dir name {e} does not start with a number.')
        if 'includedirxyz' in e:
            continue
        entries.append((numstr, rest))
    entries.sort()
    return entries

def replace_source(sourcefile: str, replacements: T.List[T.Tuple[str, str]]) -> None:
    with open(sourcefile, encoding='utf-8') as f:
        contents = f.read()
    for old_name, new_name in replacements:
        contents = contents.replace(old_name, new_name)
    with open(sourcefile, 'w', encoding='utf-8') as f:
        f.write(contents)

def condense(dirname: str) -> None:
    curdir = os.getcwd()
    os.chdir(dirname)
    entries = get_entries()
    replacements = []
    for _i, e in enumerate(entries):
        i = _i + 1
        if e[0] != i:
            old_name = str(e[0]) + ' ' + e[1]
            new_name = str(i) + ' ' + e[1]
            #print('git mv "%s" "%s"' % (old_name, new_name))
            subprocess.check_call(['git', 'mv', old_name, new_name])
            replacements.append((old_name, new_name))
            # update any appearances of old_name in expected stdout in test.json
            json = os.path.join(new_name, 'test.json')
            if os.path.isfile(json):
                replace_source(json, [(old_name, new_name)])
    os.chdir(curdir)
    replace_source('run_unittests.py', replacements)
    replace_source('run_project_tests.py', replacements)
    for f in glob('unittests/*.py'):
        replace_source(f, replacements)

if __name__ == '__main__':
    if len(sys.argv) != 1:
        raise SystemExit('This script takes no arguments.')
    for d in glob('test cases/*'):
        condense(d)
