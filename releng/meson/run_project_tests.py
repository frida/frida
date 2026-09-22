#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Copyright 2012-2021 The Meson development team

from __future__ import annotations

# Work around some pathlib bugs...
from mesonbuild import _pathlib
import sys
sys.modules['pathlib'] = _pathlib

from concurrent.futures import ProcessPoolExecutor, CancelledError
from enum import Enum
from io import StringIO
from pathlib import Path, PurePath
import argparse
import functools
import itertools
import json
import multiprocessing
import os
import re
import shlex
import shutil
import signal
import subprocess
import tempfile
import time
import typing as T
import xml.etree.ElementTree as ET
import collections
import importlib.util

from mesonbuild import build
from mesonbuild import environment
from mesonbuild import compilers
from mesonbuild import mesonlib
from mesonbuild import mlog
from mesonbuild import mtest
from mesonbuild.compilers import compiler_from_language
from mesonbuild.build import ConfigurationData
from mesonbuild.mesonlib import MachineChoice, Popen_safe, TemporaryDirectoryWinProof, setup_vsenv
from mesonbuild.mlog import blue, bold, cyan, green, red, yellow, normal_green
from mesonbuild.coredata import backendlist, version as meson_version
from mesonbuild.modules.python import PythonExternalProgram
from run_tests import (
    get_fake_options, run_configure, get_meson_script, get_backend_commands,
    get_backend_args_for_dir, Backend, ensure_backend_detects_changes,
    guess_backend, handle_meson_skip_test,
)


if T.TYPE_CHECKING:
    from types import FrameType
    from mesonbuild.environment import Environment
    from mesonbuild._typing import Protocol
    from concurrent.futures import Future

    class CompilerArgumentType(Protocol):
        cross_file: str
        native_file: str
        use_tmpdir: bool


    class ArgumentType(CompilerArgumentType):

        """Typing information for command line arguments."""

        extra_args: T.List[str]
        backend: str
        num_workers: int
        failfast: bool
        no_unittests: bool
        only: T.List[str]

ALL_TESTS = ['cmake', 'common', 'native', 'warning-meson', 'failing-meson', 'failing-build', 'failing-test',
             'keyval', 'platform-osx', 'platform-windows', 'platform-linux',
             'java', 'C#', 'vala', 'cython', 'rust', 'd', 'objective c', 'objective c++',
             'fortran', 'swift', 'cuda', 'python3', 'python', 'fpga', 'frameworks', 'nasm', 'wasm', 'wayland'
             ]


class BuildStep(Enum):
    configure = 1
    build = 2
    test = 3
    install = 4
    clean = 5
    validate = 6


class TestResult(BaseException):
    def __init__(self, cicmds: T.List[str]) -> None:
        self.msg    = ''  # empty msg indicates test success
        self.stdo   = ''
        self.stde   = ''
        self.mlog   = ''
        self.cicmds = cicmds
        self.conftime:  float = 0
        self.buildtime: float = 0
        self.testtime:  float = 0

    def add_step(self, step: BuildStep, stdo: str, stde: str, mlog: str = '', time: float = 0) -> None:
        self.step = step
        self.stdo += stdo
        self.stde += stde
        self.mlog += mlog
        if step == BuildStep.configure:
            self.conftime = time
        elif step == BuildStep.build:
            self.buildtime = time
        elif step == BuildStep.test:
            self.testtime = time

    def fail(self, msg: str) -> None:
        self.msg = msg

python = PythonExternalProgram(sys.executable)
python.sanity()

class InstalledFile:
    def __init__(self, raw: T.Dict[str, str]):
        self.path = raw['file']
        self.typ = raw['type']
        self.platform = raw.get('platform', None)
        self.language = raw.get('language', 'c')

        version = raw.get('version', '')
        if version:
            self.version = version.split('.')
        else:
            # split on '' will return [''], we want an empty list though
            self.version = []

    def get_path(self, compiler: str, env: environment.Environment) -> T.Optional[Path]:
        p = Path(self.path)
        canonical_compiler = compiler
        if ((compiler in ['clang-cl', 'intel-cl']) or
                (env.machines.host.is_windows() and compiler in {'pgi', 'dmd', 'ldc'})):
            canonical_compiler = 'msvc'

        python_suffix = python.info['suffix']
        python_limited_suffix = python.info['limited_api_suffix']
        has_pdb = False
        if self.language in {'c', 'cpp'}:
            has_pdb = canonical_compiler == 'msvc'
        elif self.language == 'd':
            # dmd's optlink does not generate pdb files
            has_pdb = env.coredata.compilers.host['d'].linker.id in {'link', 'lld-link'}

        # Abort if the platform does not match
        matches = {
            'msvc': canonical_compiler == 'msvc',
            'gcc': canonical_compiler != 'msvc',
            'cygwin': env.machines.host.is_cygwin(),
            '!cygwin': not env.machines.host.is_cygwin(),
        }.get(self.platform or '', True)
        if not matches:
            return None

        # Handle the different types
        if self.typ in {'py_implib', 'py_limited_implib', 'python_lib', 'python_limited_lib', 'python_file', 'python_bytecode'}:
            val = p.as_posix()
            val = val.replace('@PYTHON_PLATLIB@', python.platlib)
            val = val.replace('@PYTHON_PURELIB@', python.purelib)
            p = Path(val)
            if self.typ == 'python_file':
                return p
            if self.typ == 'python_lib':
                return p.with_suffix(python_suffix)
            if self.typ == 'python_limited_lib':
                return p.with_suffix(python_limited_suffix)
            if self.typ == 'py_implib':
                p = p.with_suffix(python_suffix)
                if env.machines.host.is_windows() and canonical_compiler == 'msvc':
                    return p.with_suffix('.lib')
                elif env.machines.host.is_windows() or env.machines.host.is_cygwin():
                    return p.with_suffix('.dll.a')
                else:
                    return None
            if self.typ == 'py_limited_implib':
                p = p.with_suffix(python_limited_suffix)
                if env.machines.host.is_windows() and canonical_compiler == 'msvc':
                    return p.with_suffix('.lib')
                elif env.machines.host.is_windows() or env.machines.host.is_cygwin():
                    return p.with_suffix('.dll.a')
                else:
                    return None
            if self.typ == 'python_bytecode':
                return p.parent / importlib.util.cache_from_source(p.name)
        elif self.typ in {'file', 'dir', 'link'}:
            return p
        elif self.typ == 'shared_lib':
            if env.machines.host.is_windows() or env.machines.host.is_cygwin():
                # Windows only has foo.dll and foo-X.dll
                if len(self.version) > 1:
                    return None
                if self.version:
                    p = p.with_name('{}-{}'.format(p.name, self.version[0]))
                return p.with_suffix('.dll')

            p = p.with_name(f'lib{p.name}')
            if env.machines.host.is_darwin():
                # MacOS only has libfoo.dylib and libfoo.X.dylib
                if len(self.version) > 1:
                    return None

                # pathlib.Path.with_suffix replaces, not appends
                suffix = '.dylib'
                if self.version:
                    suffix = '.{}{}'.format(self.version[0], suffix)
            else:
                # pathlib.Path.with_suffix replaces, not appends
                suffix = '.so'
                if self.version:
                    suffix = '{}.{}'.format(suffix, '.'.join(self.version))
            return p.with_suffix(suffix)
        elif self.typ == 'exe':
            if 'mwcc' in canonical_compiler:
                return p.with_suffix('.nef')
            elif env.machines.host.is_windows() or env.machines.host.is_cygwin():
                return p.with_suffix('.exe')
        elif self.typ == 'pdb':
            if self.version:
                p = p.with_name('{}-{}'.format(p.name, self.version[0]))
            return p.with_suffix('.pdb') if has_pdb else None
        elif self.typ in {'implib', 'implibempty'}:
            if env.machines.host.is_windows() and canonical_compiler == 'msvc':
                # only MSVC doesn't generate empty implibs
                if self.typ == 'implibempty' and compiler == 'msvc':
                    return None
                return p.parent / (re.sub(r'^lib', '', p.name) + '.lib')
            elif env.machines.host.is_windows() or env.machines.host.is_cygwin():
                return p.with_suffix('.dll.a')
            else:
                return None
        elif self.typ == 'expr':
            return Path(platform_fix_name(p.as_posix(), canonical_compiler, env))
        else:
            raise RuntimeError(f'Invalid installed file type {self.typ}')

        return p

    def get_paths(self, compiler: str, env: environment.Environment, installdir: Path) -> T.List[Path]:
        p = self.get_path(compiler, env)
        if not p:
            return []
        if self.typ == 'dir':
            abs_p = installdir / p
            if not abs_p.exists():
                raise RuntimeError(f'{p} does not exist')
            if not abs_p.is_dir():
                raise RuntimeError(f'{p} is not a directory')
            return [x.relative_to(installdir) for x in abs_p.rglob('*') if x.is_file() or x.is_symlink()]
        elif self.typ == 'link':
            abs_p = installdir / p
            if not abs_p.is_symlink():
                raise RuntimeError(f'{p} is not a symlink')
            return [p]
        else:
            return [p]

@functools.total_ordering
class TestDef:
    def __init__(self, path: Path, name: T.Optional[str], args: T.List[str], skip: bool = False, skip_category: bool = False):
        self.category = path.parts[1]
        self.path = path
        self.name = name
        self.args = args
        self.skip = skip
        self.env = os.environ.copy()
        self.installed_files: T.List[InstalledFile] = []
        self.do_not_set_opts: T.List[str] = []
        self.stdout: T.List[T.Dict[str, str]] = []
        self.skip_category = skip_category
        self.skip_expected = False

        # Always print a stack trace for Meson exceptions
        self.env['MESON_FORCE_BACKTRACE'] = '1'

    def __repr__(self) -> str:
        return '<{}: {:<48} [{}: {}] -- {}>'.format(type(self).__name__, str(self.path), self.name, self.args, self.skip)

    def display_name(self) -> mlog.TV_LoggableList:
        # Remove the redundant 'test cases' part
        section, id = self.path.parts[1:3]
        res: mlog.TV_LoggableList = [f'{section}:', bold(id)]
        if self.name:
            res += [f'   ({self.name})']
        return res

    def __lt__(self, other: object) -> bool:
        if isinstance(other, TestDef):
            # None is not sortable, so replace it with an empty string
            s_id = int(self.path.name.split(' ')[0])
            o_id = int(other.path.name.split(' ')[0])
            return (s_id, self.path, self.name or '') < (o_id, other.path, other.name or '')
        return NotImplemented

failing_testcases: T.List[str] = []
failing_logs: T.List[str] = []
print_debug = 'MESON_PRINT_TEST_OUTPUT' in os.environ
under_ci = 'CI' in os.environ
ci_is_github = 'GITHUB_ACTIONS' in os.environ
raw_ci_jobname = os.environ.get('MESON_CI_JOBNAME', None)
ci_jobname = raw_ci_jobname if raw_ci_jobname != 'thirdparty' else None
do_debug = under_ci or print_debug
no_meson_log_msg = 'No meson-log.txt found.'

host_c_compiler: T.Optional[str]   = None
compiler_id_map: T.Dict[str, str]  = {}
tool_vers_map:   T.Dict[str, str]  = {}

compile_commands:   T.List[str]
clean_commands:     T.List[str]
test_commands:      T.List[str]
install_commands:   T.List[str]
uninstall_commands: T.List[str]

backend:      'Backend'
backend_flags: T.List[str]

stop: bool = False
is_worker_process: bool = False

# Let's have colors in our CI output
if under_ci:
    def _ci_colorize_console() -> bool:
        return not is_worker_process

    mlog.colorize_console = _ci_colorize_console

class StopException(Exception):
    def __init__(self) -> None:
        super().__init__('Stopped by user')

def stop_handler(signal: int, frame: T.Optional['FrameType']) -> None:
    global stop
    stop = True
signal.signal(signal.SIGINT, stop_handler)
signal.signal(signal.SIGTERM, stop_handler)

def setup_commands(optbackend: str) -> None:
    global do_debug, backend, backend_flags
    global compile_commands, clean_commands, test_commands, install_commands, uninstall_commands
    backend, backend_flags = guess_backend(optbackend, shutil.which('msbuild'))
    compile_commands, clean_commands, test_commands, install_commands, \
        uninstall_commands = get_backend_commands(backend, do_debug)

# TODO try to eliminate or at least reduce this function
def platform_fix_name(fname: str, canonical_compiler: str, env: environment.Environment) -> str:
    if '?lib' in fname:
        if env.machines.host.is_windows() and canonical_compiler == 'msvc':
            fname = re.sub(r'lib/\?lib(.*)\.', r'bin/\1.', fname)
            fname = re.sub(r'/\?lib/', r'/bin/', fname)
        elif env.machines.host.is_windows():
            fname = re.sub(r'lib/\?lib(.*)\.', r'bin/lib\1.', fname)
            fname = re.sub(r'\?lib(.*)\.dll$', r'lib\1.dll', fname)
            fname = re.sub(r'/\?lib/', r'/bin/', fname)
        elif env.machines.host.is_cygwin():
            fname = re.sub(r'lib/\?lib(.*)\.so$', r'bin/cyg\1.dll', fname)
            fname = re.sub(r'lib/\?lib(.*)\.', r'bin/cyg\1.', fname)
            fname = re.sub(r'\?lib(.*)\.dll$', r'cyg\1.dll', fname)
            fname = re.sub(r'/\?lib/', r'/bin/', fname)
        else:
            fname = re.sub(r'\?lib', 'lib', fname)

    if fname.endswith('?so'):
        if env.machines.host.is_windows() and canonical_compiler == 'msvc':
            fname = re.sub(r'lib/([^/]*)\?so$', r'bin/\1.dll', fname)
            fname = re.sub(r'/(?:lib|)([^/]*?)\?so$', r'/\1.dll', fname)
            return fname
        elif env.machines.host.is_windows():
            fname = re.sub(r'lib/([^/]*)\?so$', r'bin/\1.dll', fname)
            fname = re.sub(r'/([^/]*?)\?so$', r'/\1.dll', fname)
            return fname
        elif env.machines.host.is_cygwin():
            fname = re.sub(r'lib/([^/]*)\?so$', r'bin/\1.dll', fname)
            fname = re.sub(r'/lib([^/]*?)\?so$', r'/cyg\1.dll', fname)
            fname = re.sub(r'/([^/]*?)\?so$', r'/\1.dll', fname)
            return fname
        elif env.machines.host.is_darwin():
            return fname[:-3] + '.dylib'
        else:
            return fname[:-3] + '.so'

    return fname

def validate_install(test: TestDef, installdir: Path, env: environment.Environment) -> str:
    ret_msg = ''
    expected_raw: T.List[Path] = []
    for i in test.installed_files:
        try:
            expected_raw += i.get_paths(host_c_compiler, env, installdir)
        except RuntimeError as err:
            ret_msg += f'Expected path error: {err}\n'
    expected = {x: False for x in expected_raw}
    found = [x.relative_to(installdir) for x in installdir.rglob('*') if x.is_file() or x.is_symlink()]
    # Mark all found files as found and detect unexpected files
    for fname in found:
        if fname not in expected:
            ret_msg += f'Extra file {fname} found.\n'
            continue
        expected[fname] = True
    # Check if expected files were found
    for p, f in expected.items():
        if not f:
            ret_msg += f'Expected file {p} missing.\n'
    # List dir content on error
    if ret_msg != '':
        ret_msg += '\nInstall dir contents:\n'
        for p in found:
            ret_msg += f'  - {p}\n'
    return ret_msg

def log_text_file(logfile: T.TextIO, testdir: Path, result: TestResult) -> None:
    logfile.write('%s\nstdout\n\n---\n' % testdir.as_posix())
    logfile.write(result.stdo)
    logfile.write('\n\n---\n\nstderr\n\n---\n')
    logfile.write(result.stde)
    logfile.write('\n\n---\n\n')
    if print_debug:
        try:
            print(result.stdo)
        except UnicodeError:
            sanitized_out = result.stdo.encode('ascii', errors='replace').decode()
            print(sanitized_out)
        try:
            print(result.stde, file=sys.stderr)
        except UnicodeError:
            sanitized_err = result.stde.encode('ascii', errors='replace').decode()
            print(sanitized_err, file=sys.stderr)


def _run_ci_include(args: T.List[str]) -> str:
    header = f'Included file {args[0]}:'
    footer = ''
    if ci_is_github:
        header = f'::group::==== {header} ===='
        footer = '::endgroup::'
    if not args:
        return 'At least one parameter required'
    try:
        data = Path(args[0]).read_text(errors='ignore', encoding='utf-8')
        return f'{header}\n{data}\n{footer}\n'
    except Exception:
        return 'Failed to open {}\n'.format(args[0])

ci_commands = {
    'ci_include': _run_ci_include
}

def run_ci_commands(raw_log: str) -> T.List[str]:
    res = []
    for l in raw_log.splitlines():
        if not l.startswith('!meson_ci!/'):
            continue
        cmd = shlex.split(l[11:])
        if not cmd or cmd[0] not in ci_commands:
            continue
        res += ['CI COMMAND {}:\n{}'.format(cmd[0], ci_commands[cmd[0]](cmd[1:]))]
    return res

class OutputMatch:
    def __init__(self, how: str, expected: str, count: int) -> None:
        self.how = how
        self.expected = expected
        self.count = count

    def match(self, actual: str) -> bool:
        if self.how == "re":
            return bool(re.match(self.expected, actual))
        return self.expected == actual

def _compare_output(expected: T.List[T.Dict[str, str]], output: str, desc: str) -> str:
    if expected:
        matches:   T.List[OutputMatch] = []
        nomatches: T.List[OutputMatch] = []
        for item in expected:
            how = item.get('match', 'literal')
            expected_line = item.get('line')
            count = int(item.get('count', -1))

            # Simple heuristic to automatically convert path separators for
            # Windows:
            #
            # Any '/' appearing before 'WARNING' or 'ERROR' (i.e. a path in a
            # filename part of a location) is replaced with '\' (in a re: '\\'
            # which matches a literal '\')
            #
            # (There should probably be a way to turn this off for more complex
            # cases which don't fit this)
            if mesonlib.is_windows():
                if how != "re":
                    sub = r'\\'
                else:
                    sub = r'\\\\'
                expected_line = re.sub(r'/(?=.*(WARNING|ERROR|DEPRECATION))', sub, expected_line)

            m = OutputMatch(how, expected_line, count)
            if count == 0:
                nomatches.append(m)
            else:
                matches.append(m)


        i = 0
        for actual in output.splitlines():
            # Verify this line does not match any unexpected lines (item.count == 0)
            for match in nomatches:
                if match.match(actual):
                    return f'unexpected "{match.expected}" found in {desc}'
            # If we matched all expected lines, continue to verify there are
            # no unexpected line. If nomatches is empty then we are done already.
            if i >= len(matches):
                if not nomatches:
                    break
                continue
            # Check if this line match current expected line
            match = matches[i]
            if match.match(actual):
                if match.count < 0:
                    # count was not specified, continue with next expected line,
                    # it does not matter if this line will be matched again or
                    # not.
                    i += 1
                else:
                    # count was specified (must be >0), continue expecting this
                    # same line. If count reached 0 we continue with next
                    # expected line but remember that this one must not match
                    # anymore.
                    match.count -= 1
                    if match.count == 0:
                        nomatches.append(match)
                        i += 1

        if i < len(matches):
            # reached the end of output without finding expected
            return f'expected "{matches[i].expected}" not found in {desc}'

    return ''

def validate_output(test: TestDef, stdo: str, stde: str) -> str:
    return _compare_output(test.stdout, stdo, 'stdout')

# There are some class variables and such that cache
# information. Clear all of these. The better solution
# would be to change the code so that no state is persisted
# but that would be a lot of work given that Meson was originally
# coded to run as a batch process.
def clear_internal_caches() -> None:
    import mesonbuild.interpreterbase
    from mesonbuild.dependencies.cmake import CMakeDependency
    from mesonbuild.mesonlib import PerMachine
    mesonbuild.interpreterbase.FeatureNew.feature_registry = {}
    CMakeDependency.class_cmakeinfo = PerMachine(None, None)

def run_test_inprocess(testdir: str) -> T.Tuple[int, str, str, str]:
    old_stdout = sys.stdout
    sys.stdout = mystdout = StringIO()
    old_stderr = sys.stderr
    sys.stderr = mystderr = StringIO()
    old_cwd = os.getcwd()
    os.chdir(testdir)
    test_log_fname = os.path.join('meson-logs', 'testlog.txt')
    try:
        returncode_test = mtest.run_with_args(['--no-rebuild'])
        if os.path.exists(test_log_fname):
            test_log = _run_ci_include([test_log_fname])
        else:
            test_log = ''
        returncode_benchmark = mtest.run_with_args(['--no-rebuild', '--benchmark', '--logbase', 'benchmarklog'])
    finally:
        sys.stdout = old_stdout
        sys.stderr = old_stderr
        os.chdir(old_cwd)
    return max(returncode_test, returncode_benchmark), mystdout.getvalue(), mystderr.getvalue(), test_log

# Build directory name must be the same so Ccache works over
# consecutive invocations.
def create_deterministic_builddir(test: TestDef, use_tmpdir: bool) -> str:
    import hashlib
    src_dir = test.path.as_posix()
    if test.name:
        src_dir += test.name
    rel_dirname = 'b ' + hashlib.sha256(src_dir.encode(errors='ignore')).hexdigest()[0:10]
    abs_pathname = os.path.join(tempfile.gettempdir() if use_tmpdir else os.getcwd(), rel_dirname)
    if os.path.exists(abs_pathname):
        mesonlib.windows_proof_rmtree(abs_pathname)
    os.mkdir(abs_pathname)
    return abs_pathname

def format_parameter_file(file_basename: str, test: TestDef, test_build_dir: str) -> Path:
    confdata = ConfigurationData()
    confdata.values = {'MESON_TEST_ROOT': (str(test.path.absolute()), 'base directory of current test')}

    template = test.path / (file_basename + '.in')
    destination = Path(test_build_dir) / file_basename
    mesonlib.do_conf_file(str(template), str(destination), confdata, 'meson')

    return destination

def detect_parameter_files(test: TestDef, test_build_dir: str) -> T.Tuple[Path, Path]:
    nativefile = test.path / 'nativefile.ini'
    crossfile = test.path / 'crossfile.ini'

    if os.path.exists(str(test.path / 'nativefile.ini.in')):
        nativefile = format_parameter_file('nativefile.ini', test, test_build_dir)

    if os.path.exists(str(test.path / 'crossfile.ini.in')):
        crossfile = format_parameter_file('crossfile.ini', test, test_build_dir)

    return nativefile, crossfile

# In previous python versions the global variables are lost in ProcessPoolExecutor.
# So, we use this tuple to restore some of them
class GlobalState(T.NamedTuple):
    compile_commands:   T.List[str]
    clean_commands:     T.List[str]
    test_commands:      T.List[str]
    install_commands:   T.List[str]
    uninstall_commands: T.List[str]

    backend:      'Backend'
    backend_flags: T.List[str]

    host_c_compiler: T.Optional[str]

def run_test(test: TestDef,
             extra_args: T.List[str],
             should_fail: str,
             use_tmp: bool,
             state: T.Optional[GlobalState] = None) -> T.Optional[TestResult]:
    # Unpack the global state
    global compile_commands, clean_commands, test_commands, install_commands, uninstall_commands, backend, backend_flags, host_c_compiler
    if state is not None:
        compile_commands, clean_commands, test_commands, install_commands, uninstall_commands, backend, backend_flags, host_c_compiler = state
    # Store that this is a worker process
    global is_worker_process
    is_worker_process = True
    # Setup the test environment
    assert not test.skip, 'Skipped test should not be run'
    build_dir = create_deterministic_builddir(test, use_tmp)
    try:
        with TemporaryDirectoryWinProof(prefix='i ', dir=None if use_tmp else os.getcwd()) as install_dir:
            try:
                return _run_test(test, build_dir, install_dir, extra_args, should_fail)
            except TestResult as r:
                return r
            finally:
                mlog.shutdown() # Close the log file because otherwise Windows wets itself.
    finally:
        mesonlib.windows_proof_rmtree(build_dir)

def _run_test(test: TestDef,
              test_build_dir: str,
              install_dir: str,
              extra_args: T.List[str],
              should_fail: str) -> TestResult:
    gen_start = time.time()
    # Configure in-process
    gen_args = ['setup']
    if 'prefix' not in test.do_not_set_opts:
        gen_args += ['--prefix', 'x:/usr'] if mesonlib.is_windows() else ['--prefix', '/usr']
    if 'libdir' not in test.do_not_set_opts:
        gen_args += ['--libdir', 'lib']
    gen_args += [test.path.as_posix(), test_build_dir] + backend_flags + extra_args

    nativefile, crossfile = detect_parameter_files(test, test_build_dir)

    if nativefile.exists():
        gen_args.extend(['--native-file', nativefile.as_posix()])
    if crossfile.exists():
        gen_args.extend(['--cross-file', crossfile.as_posix()])
    inprocess, res = run_configure(gen_args, env=test.env, catch_exception=True)
    returncode, stdo, stde = res
    cmd = '(inprocess) $ ' if inprocess else '$ '
    cmd += mesonlib.join_args(gen_args)
    logfile = os.path.join(test_build_dir, 'meson-logs', 'meson-log.txt')
    if os.path.exists(logfile):
        mesonlog = '\n'.join((cmd, _run_ci_include([logfile])))
    else:
        mesonlog = no_meson_log_msg
    cicmds = run_ci_commands(mesonlog)
    testresult = TestResult(cicmds)
    testresult.add_step(BuildStep.configure, '\n'.join((cmd, stdo)), stde, mesonlog, time.time() - gen_start)
    output_msg = validate_output(test, stdo, stde)
    testresult.mlog += output_msg
    if output_msg:
        testresult.fail('Unexpected output while configuring.')
        return testresult
    if should_fail == 'meson':
        if returncode == 1:
            return testresult
        elif returncode != 0:
            testresult.fail(f'Test exited with unexpected status {returncode}.')
            return testresult
        else:
            testresult.fail('Test that should have failed succeeded.')
            return testresult
    if returncode != 0:
        testresult.fail('Generating the build system failed.')
        return testresult
    builddata = build.load(test_build_dir)
    dir_args = get_backend_args_for_dir(backend, test_build_dir)

    # Build with subprocess
    def build_step() -> None:
        build_start = time.time()
        pc, o, _ = Popen_safe(compile_commands + dir_args, cwd=test_build_dir, stderr=subprocess.STDOUT)
        testresult.add_step(BuildStep.build, o, '', '', time.time() - build_start)
        if should_fail == 'build':
            if pc.returncode != 0:
                raise testresult
            testresult.fail('Test that should have failed to build succeeded.')
            raise testresult
        if pc.returncode != 0:
            testresult.fail('Compiling source code failed.')
            raise testresult

    # Touch the meson.build file to force a regenerate
    def force_regenerate() -> None:
        ensure_backend_detects_changes(backend)
        os.utime(str(test.path / 'meson.build'))

    # just test building
    build_step()

    # test that regeneration works for build step
    force_regenerate()
    build_step()  # TBD: assert nothing gets built after the regenerate?

    # test that regeneration works for test step
    force_regenerate()

    # Test in-process
    clear_internal_caches()
    test_start = time.time()
    (returncode, tstdo, tstde, test_log) = run_test_inprocess(test_build_dir)
    testresult.add_step(BuildStep.test, tstdo, tstde, test_log, time.time() - test_start)
    if should_fail == 'test':
        if returncode != 0:
            return testresult
        testresult.fail('Test that should have failed to run unit tests succeeded.')
        return testresult
    if returncode != 0:
        testresult.fail('Running unit tests failed.')
        return testresult

    # Do installation, if the backend supports it
    if install_commands:
        env = test.env.copy()
        env['DESTDIR'] = install_dir
        # Install with subprocess
        pi, o, e = Popen_safe(install_commands, cwd=test_build_dir, env=env)
        testresult.add_step(BuildStep.install, o, e)
        if pi.returncode != 0:
            testresult.fail('Running install failed.')
            return testresult

    # Clean with subprocess
    env = test.env.copy()
    pi, o, e = Popen_safe(clean_commands + dir_args, cwd=test_build_dir, env=env)
    testresult.add_step(BuildStep.clean, o, e)
    if pi.returncode != 0:
        testresult.fail('Running clean failed.')
        return testresult

    # Validate installed files
    testresult.add_step(BuildStep.install, '', '')
    if not install_commands:
        return testresult
    install_msg = validate_install(test, Path(install_dir), builddata.environment)
    if install_msg:
        testresult.fail('\n' + install_msg)
        return testresult

    return testresult


# processing of test.json 'skip_*' keys, which can appear at top level, or in
# matrix:
def _skip_keys(test_def: T.Dict) -> T.Tuple[bool, bool]:
    skip_expected = False

    # Test is expected to skip if MESON_CI_JOBNAME contains any of the list of
    # substrings
    if ('expect_skip_on_jobname' in test_def) and (ci_jobname is not None):
        skip_expected = any(s in ci_jobname for s in test_def['expect_skip_on_jobname'])

    # Test is expected to skip if os matches
    if 'expect_skip_on_os' in test_def:
        mesonenv = environment.Environment('', '', get_fake_options('/'))
        for skip_os in test_def['expect_skip_on_os']:
            if skip_os.startswith('!'):
                if mesonenv.machines.host.system != skip_os[1:]:
                    skip_expected = True
            else:
                if mesonenv.machines.host.system == skip_os:
                    skip_expected = True

    # Skip if environment variable is present
    skip = False
    if 'skip_on_env' in test_def:
        for skip_env_var in test_def['skip_on_env']:
            if skip_env_var in os.environ:
                skip = True

    return (skip, skip_expected)


def load_test_json(t: TestDef, stdout_mandatory: bool, skip_category: bool = False) -> T.List[TestDef]:
    all_tests: T.List[TestDef] = []
    test_def = {}
    test_def_file = t.path / 'test.json'
    if test_def_file.is_file():
        test_def = json.loads(test_def_file.read_text(encoding='utf-8'))

    # Handle additional environment variables
    env: T.Dict[str, str] = {}
    if 'env' in test_def:
        assert isinstance(test_def['env'], dict)
        env = test_def['env']
        for key, val in env.items():
            val = val.replace('@ROOT@', t.path.resolve().as_posix())
            val = val.replace('@PATH@', t.env.get('PATH', ''))
            env[key] = val

    # Handle installed files
    installed: T.List[InstalledFile] = []
    if 'installed' in test_def:
        installed = [InstalledFile(x) for x in test_def['installed']]

    # Handle expected output
    stdout = test_def.get('stdout', [])
    if stdout_mandatory and not stdout:
        raise RuntimeError(f"{test_def_file} must contain a non-empty stdout key")

    # Handle the do_not_set_opts list
    do_not_set_opts: T.List[str] = test_def.get('do_not_set_opts', [])

    (t.skip, t.skip_expected) = _skip_keys(test_def)

    # Skip tests if the tool requirements are not met
    if 'tools' in test_def:
        assert isinstance(test_def['tools'], dict)
        for tool, vers_req in test_def['tools'].items():
            if tool not in tool_vers_map:
                t.skip = True
            elif not mesonlib.version_compare(tool_vers_map[tool], vers_req):
                t.skip = True

    # Skip the matrix code and just update the existing test
    if 'matrix' not in test_def:
        t.env.update(env)
        t.installed_files = installed
        t.do_not_set_opts = do_not_set_opts
        t.stdout = stdout
        return [t]

    new_opt_list: T.List[T.List[T.Tuple[str, str, bool, bool]]]

    # 'matrix; entry is present, so build multiple tests from matrix definition
    opt_list: T.List[T.List[T.Tuple[str, str, bool, bool]]] = []
    matrix = test_def['matrix']
    assert "options" in matrix
    for key, val in matrix["options"].items():
        assert isinstance(val, list)
        tmp_opts: T.List[T.Tuple[str, str, bool, bool]] = []
        for i in val:
            assert isinstance(i, dict)
            assert "val" in i

            (skip, skip_expected) = _skip_keys(i)

            # Only run the test if all compiler ID's match
            if 'compilers' in i:
                for lang, id_list in i['compilers'].items():
                    if lang not in compiler_id_map or compiler_id_map[lang] not in id_list:
                        skip = True
                        break

            # Add an empty matrix entry
            if i['val'] is None:
                tmp_opts += [(key, None, skip, skip_expected)]
                continue

            tmp_opts += [(key, i['val'], skip, skip_expected)]

        if opt_list:
            new_opt_list = []
            for i in opt_list:
                for j in tmp_opts:
                    new_opt_list += [[*i, j]]
            opt_list = new_opt_list
        else:
            opt_list = [[x] for x in tmp_opts]

    # Exclude specific configurations
    if 'exclude' in matrix:
        assert isinstance(matrix['exclude'], list)
        new_opt_list = []
        for i in opt_list:
            exclude = False
            opt_tuple = [(x[0], x[1]) for x in i]
            for j in matrix['exclude']:
                ex_list = [(k, v) for k, v in j.items()]
                if all([x in opt_tuple for x in ex_list]):
                    exclude = True
                    break

            if not exclude:
                new_opt_list += [i]

        opt_list = new_opt_list

    for i in opt_list:
        name = ' '.join([f'{x[0]}={x[1]}' for x in i if x[1] is not None])
        opts = [f'-D{x[0]}={x[1]}' for x in i if x[1] is not None]
        skip = any([x[2] for x in i])
        skip_expected = any([x[3] for x in i])
        test = TestDef(t.path, name, opts, skip or t.skip, skip_category)
        test.env.update(env)
        test.installed_files = installed
        test.do_not_set_opts = do_not_set_opts
        test.stdout = stdout
        test.skip_expected = skip_expected or t.skip_expected
        all_tests.append(test)

    return all_tests


def gather_tests(testdir: Path, stdout_mandatory: bool, only: T.List[str], skip_category: bool) -> T.List[TestDef]:
    all_tests: T.List[TestDef] = []
    for t in testdir.iterdir():
        # Filter non-tests files (dot files, etc)
        if not t.is_dir() or t.name.startswith('.'):
            continue
        if t.name in {'18 includedirxyz'}:
            continue
        if only and not any(t.name.startswith(prefix) for prefix in only):
            continue
        test_def = TestDef(t, None, [], skip_category=skip_category)
        all_tests.extend(load_test_json(test_def, stdout_mandatory, skip_category))
    return sorted(all_tests)


def have_d_compiler() -> bool:
    if shutil.which("ldc2"):
        return True
    elif shutil.which("ldc"):
        return True
    elif shutil.which("gdc"):
        return True
    elif shutil.which("dmd"):
        # The Windows installer sometimes produces a DMD install
        # that exists but segfaults every time the compiler is run.
        # Don't know why. Don't know how to fix. Skip in this case.
        cp = subprocess.run(['dmd', '--version'],
                            capture_output=True)
        if cp.stdout == b'':
            return False
        return True
    return False

def have_objc_compiler(use_tmp: bool) -> bool:
    return have_working_compiler('objc', use_tmp)

def have_objcpp_compiler(use_tmp: bool) -> bool:
    return have_working_compiler('objcpp', use_tmp)

def have_cython_compiler(use_tmp: bool) -> bool:
    return have_working_compiler('cython', use_tmp)

def have_working_compiler(lang: str, use_tmp: bool) -> bool:
    with TemporaryDirectoryWinProof(prefix='b ', dir=None if use_tmp else '.') as build_dir:
        env = environment.Environment('', build_dir, get_fake_options('/'))
        try:
            compiler = compiler_from_language(env, lang, MachineChoice.HOST)
        except mesonlib.MesonException:
            return False
        if not compiler:
            return False
        env.coredata.process_compiler_options(lang, compiler, env, '')
        try:
            compiler.sanity_check(env.get_scratch_dir(), env)
        except mesonlib.MesonException:
            return False
    return True

def have_java() -> bool:
    if shutil.which('javac') and shutil.which('java'):
        return True
    return False

def skip_dont_care(t: TestDef) -> bool:
    # Everything is optional when not running on CI
    if ci_jobname is None:
        return True

    # Non-frameworks test are allowed to determine their own skipping under CI (currently)
    if not t.category.endswith('frameworks'):
        return True

    if mesonlib.is_osx() and '6 gettext' in str(t.path):
        return True

    return False

def skip_csharp(backend: Backend) -> bool:
    if backend is not Backend.ninja:
        return True
    if not shutil.which('resgen'):
        return True
    if shutil.which('mcs'):
        return False
    if shutil.which('csc'):
        # Only support VS2017 for now. Earlier versions fail
        # under CI in mysterious ways.
        try:
            stdo = subprocess.check_output(['csc', '/version'])
        except subprocess.CalledProcessError:
            return True
        # Having incrementing version numbers would be too easy.
        # Microsoft reset the versioning back to 1.0 (from 4.x)
        # when they got the Roslyn based compiler. Thus there
        # is NO WAY to reliably do version number comparisons.
        # Only support the version that ships with VS2017.
        return not stdo.startswith(b'2.')
    return True

# In Azure some setups have a broken rustc that will error out
# on all compilation attempts.

def has_broken_rustc() -> bool:
    dirname = Path('brokenrusttest')
    if dirname.exists():
        mesonlib.windows_proof_rmtree(dirname.as_posix())
    dirname.mkdir()
    sanity_file = dirname / 'sanity.rs'
    sanity_file.write_text('fn main() {\n}\n', encoding='utf-8')
    pc = subprocess.run(['rustc', '-o', 'sanity.exe', 'sanity.rs'],
                        cwd=dirname.as_posix(),
                        stdout = subprocess.DEVNULL,
                        stderr = subprocess.DEVNULL)
    mesonlib.windows_proof_rmtree(dirname.as_posix())
    return pc.returncode != 0

def should_skip_rust(backend: Backend) -> bool:
    if not shutil.which('rustc'):
        return True
    if backend is not Backend.ninja:
        return True
    if mesonlib.is_windows():
        if has_broken_rustc():
            return True
    return False

def should_skip_wayland() -> bool:
    if mesonlib.is_windows() or mesonlib.is_osx():
        return True
    if not shutil.which('wayland-scanner'):
        return True
    return False

def detect_tests_to_run(only: T.Dict[str, T.List[str]], use_tmp: bool) -> T.List[T.Tuple[str, T.List[TestDef], bool]]:
    """
    Parameters
    ----------
    only: dict of categories and list of test cases, optional
        specify names of tests to run

    Returns
    -------
    gathered_tests: list of tuple of str, list of TestDef, bool
        tests to run
    """

    skip_fortran = not(shutil.which('gfortran') or
                       shutil.which('flang') or
                       shutil.which('pgfortran') or
                       shutil.which('nagfor') or
                       shutil.which('ifort') or
                       shutil.which('ifx'))

    skip_cmake = ((os.environ.get('compiler') == 'msvc2015' and under_ci) or
                  'cmake' not in tool_vers_map or
                  not mesonlib.version_compare(tool_vers_map['cmake'], '>=3.14'))

    class TestCategory:
        def __init__(self, category: str, subdir: str, skip: bool = False, stdout_mandatory: bool = False):
            self.category = category                  # category name
            self.subdir = subdir                      # subdirectory
            self.skip = skip                          # skip condition
            self.stdout_mandatory = stdout_mandatory  # expected stdout is mandatory for tests in this category

    all_tests = [
        TestCategory('cmake', 'cmake', skip_cmake),
        TestCategory('common', 'common'),
        TestCategory('native', 'native'),
        TestCategory('warning-meson', 'warning', stdout_mandatory=True),
        TestCategory('failing-meson', 'failing', stdout_mandatory=True),
        TestCategory('failing-build', 'failing build'),
        TestCategory('failing-test',  'failing test'),
        TestCategory('keyval', 'keyval'),
        TestCategory('platform-osx', 'osx', not mesonlib.is_osx()),
        TestCategory('platform-windows', 'windows', not mesonlib.is_windows() and not mesonlib.is_cygwin()),
        TestCategory('platform-linux', 'linuxlike', mesonlib.is_osx() or mesonlib.is_windows()),
        TestCategory('java', 'java', backend is not Backend.ninja or not have_java()),
        TestCategory('C#', 'csharp', skip_csharp(backend)),
        TestCategory('vala', 'vala', backend is not Backend.ninja or not shutil.which(os.environ.get('VALAC', 'valac'))),
        TestCategory('cython', 'cython', backend is not Backend.ninja or not have_cython_compiler(options.use_tmpdir)),
        TestCategory('rust', 'rust', should_skip_rust(backend)),
        TestCategory('d', 'd', backend is not Backend.ninja or not have_d_compiler()),
        TestCategory('objective c', 'objc', backend not in (Backend.ninja, Backend.xcode) or not have_objc_compiler(options.use_tmpdir)),
        TestCategory('objective c++', 'objcpp', backend not in (Backend.ninja, Backend.xcode) or not have_objcpp_compiler(options.use_tmpdir)),
        TestCategory('fortran', 'fortran', skip_fortran or backend != Backend.ninja),
        TestCategory('swift', 'swift', backend not in (Backend.ninja, Backend.xcode) or not shutil.which('swiftc')),
        # CUDA tests on Windows: use Ninja backend:  python run_project_tests.py --only cuda --backend ninja
        TestCategory('cuda', 'cuda', backend not in (Backend.ninja, Backend.xcode) or not shutil.which('nvcc')),
        TestCategory('python3', 'python3', backend is not Backend.ninja or 'python3' not in sys.executable),
        TestCategory('python', 'python'),
        TestCategory('fpga', 'fpga', shutil.which('yosys') is None),
        TestCategory('frameworks', 'frameworks'),
        TestCategory('nasm', 'nasm'),
        TestCategory('wasm', 'wasm', shutil.which('emcc') is None or backend is not Backend.ninja),
        TestCategory('wayland', 'wayland', should_skip_wayland()),
    ]

    categories = [t.category for t in all_tests]
    assert categories == ALL_TESTS, 'argparse("--only", choices=ALL_TESTS) need to be updated to match all_tests categories'

    if only:
        for key in only.keys():
            assert key in categories, f'key `{key}` is not a recognized category'
        all_tests = [t for t in all_tests if t.category in only.keys()]

    gathered_tests = [(t.category, gather_tests(Path('test cases', t.subdir), t.stdout_mandatory, only[t.category], t.skip), t.skip) for t in all_tests]
    return gathered_tests

def run_tests(all_tests: T.List[T.Tuple[str, T.List[TestDef], bool]],
              log_name_base: str,
              failfast: bool,
              extra_args: T.List[str],
              use_tmp: bool,
              num_workers: int) -> T.Tuple[int, int, int]:
    txtname = log_name_base + '.txt'
    with open(txtname, 'w', encoding='utf-8', errors='ignore') as lf:
        return _run_tests(all_tests, log_name_base, failfast, extra_args, use_tmp, num_workers, lf)

class TestStatus(Enum):
    OK = normal_green(' [SUCCESS] ')
    SKIP = yellow(' [SKIPPED] ')
    ERROR = red('  [ERROR]  ')
    UNEXSKIP = red('[UNEXSKIP] ')
    UNEXRUN = red(' [UNEXRUN] ')
    CANCELED = cyan('[CANCELED] ')
    RUNNING = blue(' [RUNNING] ')  # Should never be actually printed
    LOG = bold('   [LOG]   ')      # Should never be actually printed

def default_print(*args: mlog.TV_Loggable, sep: str = ' ') -> None:
    print(*args, sep=sep)

safe_print = default_print

class TestRunFuture:
    def __init__(self, name: str, testdef: TestDef, future: T.Optional['Future[T.Optional[TestResult]]']) -> None:
        super().__init__()
        self.name = name
        self.testdef = testdef
        self.future = future
        self.status = TestStatus.RUNNING if self.future is not None else TestStatus.SKIP

    @property
    def result(self) -> T.Optional[TestResult]:
        return self.future.result() if self.future else None

    def log(self) -> None:
        without_install = '' if install_commands else '(without install)'
        safe_print(self.status.value, without_install, *self.testdef.display_name())

    def update_log(self, new_status: TestStatus) -> None:
        self.status = new_status
        self.log()

    def cancel(self) -> None:
        if self.future is not None and self.future.cancel():
            self.status = TestStatus.CANCELED

class LogRunFuture:
    def __init__(self, msgs: mlog.TV_LoggableList) -> None:
        self.msgs = msgs
        self.status = TestStatus.LOG

    def log(self) -> None:
        safe_print(*self.msgs, sep='')

    def cancel(self) -> None:
        pass

RunFutureUnion = T.Union[TestRunFuture, LogRunFuture]

def _run_tests(all_tests: T.List[T.Tuple[str, T.List[TestDef], bool]],
               log_name_base: str,
               failfast: bool,
               extra_args: T.List[str],
               use_tmp: bool,
               num_workers: int,
               logfile: T.TextIO) -> T.Tuple[int, int, int]:
    global stop, host_c_compiler
    xmlname = log_name_base + '.xml'
    junit_root = ET.Element('testsuites')
    conf_time:  float = 0
    build_time: float = 0
    test_time:  float = 0
    passing_tests = 0
    failing_tests = 0
    skipped_tests = 0

    print(f'\nRunning tests with {num_workers} workers')

    # Pack the global state
    state = GlobalState(compile_commands, clean_commands, test_commands, install_commands, uninstall_commands, backend, backend_flags, host_c_compiler)
    executor = ProcessPoolExecutor(max_workers=num_workers)

    futures: T.List[RunFutureUnion] = []

    # First, collect and start all tests and also queue log messages
    for name, test_cases, skipped in all_tests:
        current_suite = ET.SubElement(junit_root, 'testsuite', {'name': name, 'tests': str(len(test_cases))})
        if skipped:
            futures += [LogRunFuture(['\n', bold(f'Not running {name} tests.'), '\n'])]
        else:
            futures += [LogRunFuture(['\n', bold(f'Running {name} tests.'), '\n'])]

        for t in test_cases:
            # Jenkins screws us over by automatically sorting test cases by name
            # and getting it wrong by not doing logical number sorting.
            (testnum, testbase) = t.path.name.split(' ', 1)
            testname = '%.3d %s' % (int(testnum), testbase)
            if t.name:
                testname += f' ({t.name})'
            should_fail = ''
            suite_args = []
            if name.startswith('failing'):
                should_fail = name.split('failing-')[1]
            if name.startswith('warning'):
                suite_args = ['--fatal-meson-warnings']
                should_fail = name.split('warning-')[1]

            if skipped or t.skip:
                futures += [TestRunFuture(testname, t, None)]
                continue
            result_future = executor.submit(run_test, t, extra_args + suite_args + t.args, should_fail, use_tmp, state=state)
            futures += [TestRunFuture(testname, t, result_future)]

    # Ensure we only cancel once
    tests_canceled = False

    # Optionally enable the tqdm progress bar, but only if there is at least
    # one LogRunFuture and one TestRunFuture
    global safe_print
    futures_iter: T.Iterable[RunFutureUnion] = futures
    if len(futures) > 2 and sys.stdout.isatty():
        try:
            from tqdm import tqdm
            futures_iter = tqdm(futures, desc='Running tests', unit='test')

            def tqdm_print(*args: mlog.TV_Loggable, sep: str = ' ') -> None:
                tqdm.write(sep.join([str(x) for x in args]))

            safe_print = tqdm_print
        except ImportError:
            pass

    # Wait and handle the test results and print the stored log output
    for f in futures_iter:
        # Just a log entry to print something to stdout
        sys.stdout.flush()
        if isinstance(f, LogRunFuture):
            f.log()
            continue

        # Actual Test run
        testname = f.name
        t        = f.testdef
        try:
            result = f.result
        except (CancelledError, KeyboardInterrupt):
            f.status = TestStatus.CANCELED

        if stop and not tests_canceled:
            num_running = sum(1 if f2.status is TestStatus.RUNNING  else 0 for f2 in futures)
            for f2 in futures:
                f2.cancel()
            executor.shutdown()
            num_canceled = sum(1 if f2.status is TestStatus.CANCELED else 0 for f2 in futures)
            safe_print(f'\nCanceled {num_canceled} out of {num_running} running tests.')
            safe_print(f'Finishing the remaining {num_running - num_canceled} tests.\n')
            tests_canceled = True

        # Handle canceled tests
        if f.status is TestStatus.CANCELED:
            f.log()
            continue

        # Handle skipped tests
        if result is None:
            # skipped due to skipped category skip or 'tools:' or 'skip_on_env:'
            is_skipped = True
            skip_reason = 'not run because preconditions were not met'
            skip_as_expected = True
        else:
            # skipped due to test outputting 'MESON_SKIP_TEST'
            is_skipped, skip_reason = handle_meson_skip_test(result.stdo)
            if not skip_dont_care(t):
                skip_as_expected = (is_skipped == t.skip_expected)
            else:
                skip_as_expected = True

        if is_skipped:
            skipped_tests += 1

        if is_skipped and skip_as_expected:
            f.update_log(TestStatus.SKIP)
            if not t.skip_category:
                safe_print(bold('Reason:'), skip_reason)
            current_test = ET.SubElement(current_suite, 'testcase', {'name': testname, 'classname': t.category})
            ET.SubElement(current_test, 'skipped', {})
            continue

        if not skip_as_expected:
            failing_tests += 1
            if is_skipped:
                skip_msg = f'Test asked to be skipped ({skip_reason}), but was not expected to'
                status = TestStatus.UNEXSKIP
            else:
                skip_msg = 'Test ran, but was expected to be skipped'
                status = TestStatus.UNEXRUN
            result.msg = f"{skip_msg} for MESON_CI_JOBNAME '{ci_jobname}'"

            f.update_log(status)
            safe_print(bold('Reason:'), result.msg)
            current_test = ET.SubElement(current_suite, 'testcase', {'name': testname, 'classname': t.category})
            ET.SubElement(current_test, 'failure', {'message': result.msg})
            continue

        # Handle Failed tests
        if result.msg != '':
            f.update_log(TestStatus.ERROR)
            safe_print(bold('During:'), result.step.name)
            safe_print(bold('Reason:'), result.msg)
            failing_tests += 1
            # Append a visual separator for the different test cases
            cols = shutil.get_terminal_size((100, 20)).columns
            name_str = ' '.join([str(x) for x in f.testdef.display_name()])
            name_len = len(re.sub(r'\x1B[^m]+m', '', name_str))  # Do not count escape sequences
            left_w = (cols // 2) - (name_len // 2) - 1
            left_w = max(3, left_w)
            right_w = cols - left_w - name_len - 2
            right_w = max(3, right_w)
            failing_testcases.append(name_str)
            failing_logs.append(f'\n\x1b[31m{"="*left_w}\x1b[0m {name_str} \x1b[31m{"="*right_w}\x1b[0m\n')
            _during = bold('Failed during:')
            _reason = bold('Reason:')
            failing_logs.append(f'{_during} {result.step.name}\n{_reason} {result.msg}\n')
            if result.step == BuildStep.configure and result.mlog != no_meson_log_msg:
                # For configure failures, instead of printing stdout,
                # print the meson log if available since it's a superset
                # of stdout and often has very useful information.
                failing_logs.append(result.mlog)
            elif under_ci:
                # Always print the complete meson log when running in
                # a CI. This helps debugging issues that only occur in
                # a hard to reproduce environment
                failing_logs.append(result.mlog)
                failing_logs.append(result.stdo)
            else:
                failing_logs.append(result.stdo)
            for cmd_res in result.cicmds:
                failing_logs.append(cmd_res)
            failing_logs.append(result.stde)
            if failfast:
                safe_print("Cancelling the rest of the tests")
                for f2 in futures:
                    f2.cancel()
        else:
            f.update_log(TestStatus.OK)
            passing_tests += 1
        conf_time += result.conftime
        build_time += result.buildtime
        test_time += result.testtime
        total_time = conf_time + build_time + test_time
        log_text_file(logfile, t.path, result)
        current_test = ET.SubElement(
            current_suite,
            'testcase',
            {'name': testname, 'classname': t.category, 'time': '%.3f' % total_time}
        )
        if result.msg != '':
            ET.SubElement(current_test, 'failure', {'message': result.msg})
        stdoel = ET.SubElement(current_test, 'system-out')
        stdoel.text = result.stdo
        stdeel = ET.SubElement(current_test, 'system-err')
        stdeel.text = result.stde

    # Reset, just in case
    safe_print = default_print

    print()
    print("Total configuration time: %.2fs" % conf_time)
    print("Total build time:         %.2fs" % build_time)
    print("Total test time:          %.2fs" % test_time)
    ET.ElementTree(element=junit_root).write(xmlname, xml_declaration=True, encoding='UTF-8')
    return passing_tests, failing_tests, skipped_tests

def check_meson_commands_work(use_tmpdir: bool, extra_args: T.List[str]) -> None:
    global backend, compile_commands, test_commands, install_commands
    testdir = PurePath('test cases', 'common', '1 trivial').as_posix()
    meson_commands = mesonlib.python_command + [get_meson_script()]
    with TemporaryDirectoryWinProof(prefix='b ', dir=None if use_tmpdir else '.') as build_dir:
        print('Checking that configuring works...')
        gen_cmd = meson_commands + ['setup' , testdir, build_dir] + backend_flags + extra_args
        pc, o, e = Popen_safe(gen_cmd)
        if pc.returncode != 0:
            raise RuntimeError(f'Failed to configure {testdir!r}:\n{e}\n{o}')
        print('Checking that introspect works...')
        pc, o, e = Popen_safe(meson_commands + ['introspect', '--targets'], cwd=build_dir)
        json.loads(o)
        if pc.returncode != 0:
            raise RuntimeError(f'Failed to introspect --targets {testdir!r}:\n{e}\n{o}')
        print('Checking that building works...')
        dir_args = get_backend_args_for_dir(backend, build_dir)
        pc, o, e = Popen_safe(compile_commands + dir_args, cwd=build_dir)
        if pc.returncode != 0:
            raise RuntimeError(f'Failed to build {testdir!r}:\n{e}\n{o}')
        print('Checking that testing works...')
        pc, o, e = Popen_safe(test_commands, cwd=build_dir)
        if pc.returncode != 0:
            raise RuntimeError(f'Failed to test {testdir!r}:\n{e}\n{o}')
        if install_commands:
            print('Checking that installing works...')
            pc, o, e = Popen_safe(install_commands, cwd=build_dir)
            if pc.returncode != 0:
                raise RuntimeError(f'Failed to install {testdir!r}:\n{e}\n{o}')


def detect_system_compiler(options: 'CompilerArgumentType', quick: bool = False) -> None:
    global host_c_compiler, compiler_id_map

    fake_opts = get_fake_options('/')
    if options.cross_file:
        fake_opts.cross_file = [options.cross_file]
    if options.native_file:
        fake_opts.native_file = [options.native_file]

    env = environment.Environment('', '', fake_opts)

    if not quick:
        print_compilers(env, MachineChoice.HOST)
        if options.cross_file:
            print_compilers(env, MachineChoice.BUILD)
        langs = sorted(compilers.all_languages)
    else:
        langs = ['c']

    for lang in langs:
        try:
            comp = compiler_from_language(env, lang, MachineChoice.HOST)
            # note compiler id for later use with test.json matrix
            compiler_id_map[lang] = comp.get_id()
        except mesonlib.MesonException:
            comp = None

        # note C compiler for later use by platform_fix_name()
        if lang == 'c':
            if comp:
                host_c_compiler = comp.get_id()
            else:
                raise RuntimeError("Could not find C compiler.")


def print_compilers(env: 'Environment', machine: MachineChoice) -> None:
    print()
    print(f'{machine.get_lower_case_name()} machine compilers')
    print()
    for lang in sorted(compilers.all_languages):
        try:
            comp = compiler_from_language(env, lang, machine)
            details = '{:<10} {} {}'.format('[' + comp.get_id() + ']', ' '.join(comp.get_exelist()), comp.get_version_string())
        except mesonlib.MesonException:
            details = '[not found]'
        print(f'{lang:<7}: {details}')

class ToolInfo(T.NamedTuple):
    tool: str
    args: T.List[str]
    regex: T.Pattern
    match_group: int

def print_tool_versions() -> None:
    tools: T.List[ToolInfo] = [
        ToolInfo(
            'ninja',
            ['--version'],
            re.compile(r'^([0-9]+(\.[0-9]+)*(-[a-z0-9]+)?)$'),
            1,
        ),
        ToolInfo(
            'cmake',
            ['--version'],
            re.compile(r'^cmake version ([0-9]+(\.[0-9]+)*(-[a-z0-9]+)?)$'),
            1,
        ),
        ToolInfo(
            'hotdoc',
            ['--version'],
            re.compile(r'^([0-9]+(\.[0-9]+)*(-[a-z0-9]+)?)$'),
            1,
        ),
    ]

    def get_version(t: ToolInfo) -> str:
        exe = shutil.which(t.tool)
        if not exe:
            return 'not found'

        args = [t.tool] + t.args
        pc, o, e = Popen_safe(args)
        if pc.returncode != 0:
            return f'{exe} (invalid {t.tool} executable)'
        for i in o.split('\n'):
            i = i.strip('\n\r\t ')
            m = t.regex.match(i)
            if m is not None:
                tool_vers_map[t.tool] = m.group(t.match_group)
                return '{} ({})'.format(exe, m.group(t.match_group))

        return f'{exe} (unknown)'

    print()
    print('tools')
    print()

    max_width = max([len(x.tool) for x in tools] + [7])
    for tool in tools:
        print('{0:<{2}}: {1}'.format(tool.tool, get_version(tool), max_width))
    print()

tmpdir = list(Path('.').glob('test cases/**/*install functions and follow symlinks'))
assert(len(tmpdir) == 1)
symlink_test_dir = tmpdir[0]
symlink_file1 = symlink_test_dir / 'foo/link1'
symlink_file2 = symlink_test_dir / 'foo/link2.h'
del tmpdir

def clear_transitive_files() -> None:
    a = Path('test cases/common')
    for d in a.glob('*subproject subdir/subprojects/subsubsub*'):
        if d.is_dir():
            mesonlib.windows_proof_rmtree(str(d))
        else:
            mesonlib.windows_proof_rm(str(d))
    try:
        symlink_file1.unlink()
    except FileNotFoundError:
        pass
    try:
        symlink_file2.unlink()
    except FileNotFoundError:
        pass

def setup_symlinks() -> None:
    try:
        symlink_file1.symlink_to('file1')
        symlink_file2.symlink_to('file1')
    except OSError:
        print('symlinks are not supported on this system')

if __name__ == '__main__':
    if under_ci and not raw_ci_jobname:
        raise SystemExit('Running under CI but $MESON_CI_JOBNAME is not set (set to "thirdparty" if you are running outside of the github org)')

    setup_vsenv()

    try:
        # This fails in some CI environments for unknown reasons.
        num_workers = multiprocessing.cpu_count()
    except Exception as e:
        print('Could not determine number of CPUs due to the following reason:', str(e))
        print('Defaulting to using only two processes')
        num_workers = 2

    if num_workers > 64:
        # Too much parallelism seems to trigger a potential Python bug:
        # https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1004107
        num_workers = 64

    parser = argparse.ArgumentParser(description="Run the test suite of Meson.")
    parser.add_argument('extra_args', nargs='*',
                        help='arguments that are passed directly to Meson (remember to have -- before these).')
    parser.add_argument('--backend', dest='backend', choices=backendlist)
    parser.add_argument('-j', dest='num_workers', type=int, default=num_workers,
                        help=f'Maximum number of parallel tests (default {num_workers})')
    parser.add_argument('--failfast', action='store_true',
                        help='Stop running if test case fails')
    parser.add_argument('--no-unittests', action='store_true',
                        help='Not used, only here to simplify run_tests.py')
    parser.add_argument('--only', default=[],
                        help='name of test(s) to run, in format "category[/name]" where category is one of: ' + ', '.join(ALL_TESTS), nargs='+')
    parser.add_argument('--cross-file', action='store', help='File describing cross compilation environment.')
    parser.add_argument('--native-file', action='store', help='File describing native compilation environment.')
    parser.add_argument('--use-tmpdir', action='store_true', help='Use tmp directory for temporary files.')
    options = T.cast('ArgumentType', parser.parse_args())

    if options.cross_file:
        options.extra_args += ['--cross-file', options.cross_file]
    if options.native_file:
        options.extra_args += ['--native-file', options.native_file]

    clear_transitive_files()
    setup_symlinks()
    mesonlib.set_meson_command(get_meson_script())

    print('Meson build system', meson_version, 'Project Tests')
    print('Using python', sys.version.split('\n')[0], f'({sys.executable!r})')
    if 'VSCMD_VER' in os.environ:
        print('VSCMD version', os.environ['VSCMD_VER'])
    setup_commands(options.backend)
    detect_system_compiler(options)
    print_tool_versions()
    script_dir = os.path.split(__file__)[0]
    if script_dir != '':
        os.chdir(script_dir)
    check_meson_commands_work(options.use_tmpdir, options.extra_args)
    only = collections.defaultdict(list)
    for i in options.only:
        try:
            cat, case = i.split('/')
            only[cat].append(case)
        except ValueError:
            only[i].append('')
    try:
        all_tests = detect_tests_to_run(only, options.use_tmpdir)
        res = run_tests(all_tests, 'meson-test-run', options.failfast, options.extra_args, options.use_tmpdir, options.num_workers)
        (passing_tests, failing_tests, skipped_tests) = res
    except StopException:
        pass
    if failing_tests > 0:
        print('\nMesonlogs of failing tests\n')
        for l in failing_logs:
            try:
                print(l, '\n')
            except UnicodeError:
                print(l.encode('ascii', errors='replace').decode(), '\n')
    print()
    print('Total passed tests: ', green(str(passing_tests)))
    print('Total failed tests: ', red(str(failing_tests)))
    print('Total skipped tests:', yellow(str(skipped_tests)))
    if failing_tests > 0:
        print('\nAll failures:')
        for c in failing_testcases:
            print(f'  -> {c}')
    for name, dirs, _ in all_tests:
        dir_names = list({x.path.name for x in dirs})
        for k, g in itertools.groupby(dir_names, key=lambda x: x.split()[0]):
            tests = list(g)
            if len(tests) != 1:
                print('WARNING: The {} suite contains duplicate "{}" tests: "{}"'.format(name, k, '", "'.join(tests)))
    clear_transitive_files()
    raise SystemExit(failing_tests)
