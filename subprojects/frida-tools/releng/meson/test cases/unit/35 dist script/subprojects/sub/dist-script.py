#!/usr/bin/env python3

import os
import pathlib
import shlex
import subprocess
import sys

assert sys.argv[1] == 'success'

source_root = pathlib.Path(os.environ['MESON_PROJECT_DIST_ROOT'])
mesonrewrite = shlex.split(os.environ['MESONREWRITE'])
rewrite_cmd = ['kwargs', 'set', 'project', '/', 'version', 'release']

subprocess.run([*mesonrewrite, '-s', source_root, *rewrite_cmd], check=True)

modfile = source_root / 'prog.c'
with modfile.open('w') as f:
    f.write('int main(){return 0;}')
