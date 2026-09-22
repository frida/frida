#!/usr/bin/env python3


# Generates a static library, object file, source
# file and a header file.

import sys, os
import subprocess

with open(sys.argv[1]) as f:
    funcname = f.readline().strip()
outdir = sys.argv[2]
buildtype_args = sys.argv[3]
compiler_type = sys.argv[4]
compiler = sys.argv[5:]

if not os.path.isdir(outdir):
    print('Outdir does not exist.')
    sys.exit(1)

if compiler_type == 'msvc':
    libsuffix = '.lib'
    is_vs = True
    if any(['clang-cl' in c for c in compiler]):
        linker = 'llvm-lib'
    else:
        linker = 'lib'
else:
    libsuffix = '.a'
    is_vs = False
    linker = 'ar'

objsuffix = '.o'

outo = os.path.join(outdir, funcname + objsuffix)
outa = os.path.join(outdir, funcname + libsuffix)
outh = os.path.join(outdir, funcname + '.h')
outc = os.path.join(outdir, funcname + '.c')

tmpc = 'diibadaaba.c'
tmpo = 'diibadaaba' + objsuffix

with open(outc, 'w') as f:
    f.write('''#include"{}.h"
int {}_in_src(void) {{
  return 0;
}}
'''.format(funcname, funcname))

with open(outh, 'w') as f:
    f.write('''#pragma once
int {}_in_lib(void);
int {}_in_obj(void);
int {}_in_src(void);
'''.format(funcname, funcname, funcname))

with open(tmpc, 'w') as f:
    f.write('''int %s_in_obj(void) {
  return 0;
}
''' % funcname)

if is_vs:
    subprocess.check_call(compiler + ['/nologo', '/c', buildtype_args, '/Fo' + outo, tmpc])
else:
    subprocess.check_call(compiler + ['-c', '-o', outo, tmpc])

with open(tmpc, 'w') as f:
    f.write('''int %s_in_lib() {
  return 0;
}
''' % funcname)

if is_vs:
    subprocess.check_call(compiler + ['/nologo', '/c', '/Fo' + tmpo, tmpc])
    subprocess.check_call([linker, '/NOLOGO', '/OUT:' + outa, tmpo])
else:
    subprocess.check_call(compiler + ['-c', '-o', tmpo, tmpc])
    subprocess.check_call([linker, 'csr', outa, tmpo])

os.unlink(tmpo)
os.unlink(tmpc)
