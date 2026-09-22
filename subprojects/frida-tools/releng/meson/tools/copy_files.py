#!/usr/bin/env python3
# Copyright 2018 The Meson development team

'''
Copy files
'''

import argparse
import shutil
import typing as T
from pathlib import Path

PathLike = T.Union[Path,str]

def copy_files(files: T.List[str], input_dir: PathLike, output_dir: PathLike) -> None:
    if not input_dir:
        raise ValueError(f'Input directory value is not set')
    if not output_dir:
        raise ValueError(f'Output directory value is not set')

    input_dir = Path(input_dir).resolve()
    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    for f in files:
        if (input_dir/f).is_dir():
            shutil.copytree(input_dir/f, output_dir/f)
        else:
            shutil.copy2(input_dir/f, output_dir/f)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Copy files')
    parser.add_argument('files', metavar='FILE', nargs='*')
    parser.add_argument('-C', dest='input_dir', required=True)
    parser.add_argument('--output-dir', required=True)

    args = parser.parse_args()

    copy_files(files=args.files,
               input_dir=args.input_dir,
               output_dir=args.output_dir)
