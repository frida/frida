#!/usr/bin/env python3

import argparse
from pathlib import Path
import shutil
import sys
import tempfile
import zipapp

parser = argparse.ArgumentParser()
parser.add_argument('source', nargs='?', default='.', help='Source directory')
parser.add_argument('--outfile', default='meson.pyz', help='Output file for the zipapp')
parser.add_argument('--interpreter', default='/usr/bin/env python3', help='The name of the Python interpreter to use')
parser.add_argument('--compress', action='store_true', default=False, help='Compress files')

options = parser.parse_args(sys.argv[1:])

source = Path(options.source).resolve()

with tempfile.TemporaryDirectory() as d:
    shutil.copy2(source / 'meson.py', Path(d, '__main__.py'))
    shutil.copytree(source / 'mesonbuild', Path(d, 'mesonbuild'))
    zipapp.create_archive(d, interpreter=options.interpreter, target=options.outfile, compressed=options.compress)
