#!/usr/bin/env python3

import shutil, sys, subprocess, argparse, pathlib

parser = argparse.ArgumentParser()

parser.add_argument('--private-dir', required=True)
parser.add_argument('-o', nargs='+', required=True)
parser.add_argument('cmparr', nargs='+')

contents = ['''#include<stdio.h>

void flob_1() {
    printf("Now flobbing #1.\\n");
}
''', '''#include<stdio.h>

void flob_2() {
    printf("Now flobbing #2.\\n");
}
''']

def generate_lib_gnulike(outfile, c_file, private_dir, compiler_array):
    if shutil.which('ar'):
        static_linker = 'ar'
    elif shutil.which('llvm-ar'):
        static_linker = 'llvm-ar'
    elif shutil.which('gcc-ar'):
        static_linker = 'gcc-ar'
    else:
        sys.exit('Could not detect a static linker.')
    o_file = c_file.with_suffix('.o')
    compile_cmd = compiler_array + ['-c', '-g', '-O2', '-o', str(o_file), str(c_file)]
    subprocess.check_call(compile_cmd)
    out_file = pathlib.Path(outfile)
    if out_file.exists():
        out_file.unlink()
    link_cmd = [static_linker, 'csr', outfile, str(o_file)]
    subprocess.check_call(link_cmd)
    return 0


def generate_lib_msvc(outfile, c_file, private_dir, compiler_array):
    static_linker = 'lib'
    o_file = c_file.with_suffix('.obj')
    compile_cmd = compiler_array + ['/MDd',
                                    '/nologo',
                                    '/ZI',
                                    '/Ob0',
                                    '/Od',
                                    '/c',
                                    '/Fo' + str(o_file),
                                    str(c_file)]
    subprocess.check_call(compile_cmd)
    out_file = pathlib.Path(outfile)
    if out_file.exists():
        out_file.unlink()
    link_cmd = [static_linker,
                '/nologo',
                '/OUT:' + str(outfile),
                str(o_file)]
    subprocess.check_call(link_cmd)
    return 0

def generate_lib(outfiles, private_dir, compiler_array):
    private_dir = pathlib.Path(private_dir)
    if not private_dir.exists():
        private_dir.mkdir()

    for i, content in enumerate(contents):
        c_file = private_dir / ('flob_' + str(i + 1) + '.c')
        c_file.write_text(content)
        outfile = outfiles[i]

        cl_found = False
        for cl_arg in compiler_array:
            if (cl_arg.endswith('cl') or cl_arg.endswith('cl.exe')) and 'clang-cl' not in cl_arg:
                ret = generate_lib_msvc(outfile, c_file, private_dir, compiler_array)
                if ret > 0:
                    return ret
                else:
                    cl_found = True
                    break
        if not cl_found:
            ret = generate_lib_gnulike(outfile, c_file, private_dir, compiler_array)
            if ret > 0:
                return ret
    return 0

if __name__ == '__main__':
    options = parser.parse_args()
    sys.exit(generate_lib(options.o, options.private_dir, options.cmparr))
