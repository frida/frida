#!/usr/bin/env python3

import sys, os, subprocess

def generate(infile, outfile, fallback):
    workdir = os.path.split(infile)[0]
    if workdir == '':
        workdir = '.'
    try:
        version = subprocess.check_output(['git', 'describe'], cwd=workdir).decode().strip()
    except (subprocess.CalledProcessError, OSError, UnicodeDecodeError):
        version = fallback
    with open(infile) as f:
        newdata = f.read().replace('@VERSION@', version)
    try:
        with open(outfile) as f:
            olddata = f.read()
        if olddata == newdata:
            return
    except OSError:
        pass
    with open(outfile, 'w') as f:
        f.write(newdata)

if __name__ == '__main__':
    infile = sys.argv[1]
    outfile = sys.argv[2]
    fallback = sys.argv[3]
    generate(infile, outfile, fallback)
