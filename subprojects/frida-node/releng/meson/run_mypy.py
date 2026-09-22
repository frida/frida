#!/usr/bin/env python3

from pathlib import Path
import argparse
import os
import subprocess
import sys
import typing as T

from mesonbuild.mesonlib import version_compare

modules = [
    # fully typed submodules
    # 'mesonbuild/ast/',
    'mesonbuild/cargo/',
    'mesonbuild/cmake/',
    'mesonbuild/compilers/',
    'mesonbuild/dependencies/',
    'mesonbuild/interpreter/primitives/',
    'mesonbuild/interpreterbase/',
    'mesonbuild/linkers/',
    'mesonbuild/scripts/',
    'mesonbuild/templates/',
    'mesonbuild/wrap/',

    # specific files
    'mesonbuild/ast/introspection.py',
    'mesonbuild/ast/printer.py',
    'mesonbuild/ast/postprocess.py',
    'mesonbuild/ast/visitor.py',
    'mesonbuild/arglist.py',
    'mesonbuild/backend/backends.py',
    'mesonbuild/backend/nonebackend.py',
    # 'mesonbuild/coredata.py',
    'mesonbuild/depfile.py',
    'mesonbuild/envconfig.py',
    'mesonbuild/interpreter/compiler.py',
    'mesonbuild/interpreter/mesonmain.py',
    'mesonbuild/interpreter/interpreterobjects.py',
    'mesonbuild/interpreter/type_checking.py',
    'mesonbuild/mcompile.py',
    'mesonbuild/mdevenv.py',
    'mesonbuild/utils/core.py',
    'mesonbuild/utils/platform.py',
    'mesonbuild/utils/universal.py',
    'mesonbuild/mconf.py',
    'mesonbuild/mdist.py',
    'mesonbuild/minit.py',
    'mesonbuild/minstall.py',
    'mesonbuild/mintro.py',
    'mesonbuild/mlog.py',
    'mesonbuild/msubprojects.py',
    'mesonbuild/modules/__init__.py',
    'mesonbuild/modules/cuda.py',
    'mesonbuild/modules/external_project.py',
    'mesonbuild/modules/fs.py',
    'mesonbuild/modules/gnome.py',
    'mesonbuild/modules/i18n.py',
    'mesonbuild/modules/icestorm.py',
    'mesonbuild/modules/java.py',
    'mesonbuild/modules/keyval.py',
    'mesonbuild/modules/modtest.py',
    'mesonbuild/modules/pkgconfig.py',
    'mesonbuild/modules/qt.py',
    'mesonbuild/modules/qt4.py',
    'mesonbuild/modules/qt5.py',
    'mesonbuild/modules/qt6.py',
    'mesonbuild/modules/rust.py',
    'mesonbuild/modules/simd.py',
    'mesonbuild/modules/sourceset.py',
    'mesonbuild/modules/wayland.py',
    'mesonbuild/modules/windows.py',
    'mesonbuild/mparser.py',
    'mesonbuild/msetup.py',
    'mesonbuild/mtest.py',
    'mesonbuild/optinterpreter.py',
    'mesonbuild/programs.py',
]
additional = [
    'run_mypy.py',
    'run_project_tests.py',
    'run_single_test.py',
    'tools',
    'docs/genrefman.py',
    'docs/refman',
]

if os.name == 'posix':
    modules.append('mesonbuild/utils/posix.py')
elif os.name == 'nt':
    modules.append('mesonbuild/utils/win32.py')

def check_mypy() -> None:
    try:
        import mypy
    except ImportError:
        print('Failed import mypy')
        sys.exit(1)
    from mypy.version import __version__ as mypy_version
    if not version_compare(mypy_version, '>=0.812'):
        print('mypy >=0.812 is required, older versions report spurious errors')
        sys.exit(1)

def main() -> int:
    check_mypy()

    root = Path(__file__).absolute().parent

    parser = argparse.ArgumentParser(description='Process some integers.')
    parser.add_argument('files', nargs='*')
    parser.add_argument('--mypy', help='path to mypy executable')
    parser.add_argument('-q', '--quiet', action='store_true', help='do not print informational messages')
    parser.add_argument('-p', '--pretty', action='store_true', help='pretty print mypy errors')
    parser.add_argument('-C', '--clear', action='store_true', help='clear the terminal before running mypy')
    parser.add_argument('--allver', action='store_true', help='Check all supported versions of python')

    opts, args = parser.parse_known_args()
    if opts.pretty:
        args.append('--pretty')

    if opts.clear:
        print('\x1bc', end='', flush=True)

    to_check = [] # type: T.List[str]
    additional_to_check = [] # type: T.List[str]
    if opts.files:
        for f in opts.files:
            if f in modules:
                to_check.append(f)
            elif any(f.startswith(i) for i in modules):
                to_check.append(f)
            elif f in additional:
                additional_to_check.append(f)
            elif any(f.startswith(i) for i in additional):
                additional_to_check.append(f)
            else:
                if not opts.quiet:
                    print(f'skipping {f!r} because it is not yet typed')
    else:
        to_check.extend(modules)
        additional_to_check.extend(additional)

    if to_check:
        command = [opts.mypy] if opts.mypy else [sys.executable, '-m', 'mypy']
        if not opts.quiet:
            print('Running mypy (this can take some time) ...')
        retcode = subprocess.run(command + args + to_check + additional_to_check, cwd=root).returncode
        if opts.allver and retcode == 0:
            for minor in range(7, sys.version_info[1]):
                if not opts.quiet:
                    print(f'Checking mypy with python version: 3.{minor}')
                p = subprocess.run(command + args + to_check + [f'--python-version=3.{minor}'], cwd=root)
                if p.returncode != 0:
                    retcode = p.returncode
        return retcode
    else:
        if not opts.quiet:
            print('nothing to do...')
        return 0

if __name__ == '__main__':
    sys.exit(main())
