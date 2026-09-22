# SPDX-License-Identifier: Apache-2.0
# Copyright 2016-2021 The Meson development team
# Copyright © 2024 Intel Corporation

from __future__ import annotations
from pathlib import PurePath
from unittest import mock, TestCase, SkipTest
import json
import io
import os
import re
import subprocess
import sys
import shutil
import tempfile
import typing as T

import mesonbuild.mlog
import mesonbuild.depfile
import mesonbuild.dependencies.base
import mesonbuild.dependencies.factory
import mesonbuild.compilers
import mesonbuild.envconfig
import mesonbuild.environment
import mesonbuild.coredata
import mesonbuild.modules.gnome
from mesonbuild.mesonlib import (
    is_cygwin, join_args, split_args, windows_proof_rmtree, python_command
)
import mesonbuild.modules.pkgconfig


from run_tests import (
    Backend, ensure_backend_detects_changes, get_backend_commands,
    get_builddir_target_args, get_meson_script, run_configure_inprocess,
    run_mtest_inprocess, handle_meson_skip_test,
)


# magic attribute used by unittest.result.TestResult._is_relevant_tb_level
# This causes tracebacks to hide these internal implementation details,
# e.g. for assertXXX helpers.
__unittest = True

class BasePlatformTests(TestCase):
    prefix = '/usr'
    libdir = 'lib'

    def setUp(self):
        super().setUp()
        self.maxDiff = None
        src_root = str(PurePath(__file__).parents[1])
        self.src_root = src_root
        # Get the backend
        self.backend_name = os.environ['MESON_UNIT_TEST_BACKEND']
        backend_type = 'vs' if self.backend_name.startswith('vs') else self.backend_name
        self.backend = getattr(Backend, backend_type)
        self.meson_args = ['--backend=' + self.backend_name]
        self.meson_native_files = []
        self.meson_cross_files = []
        self.meson_command = python_command + [get_meson_script()]
        self.setup_command = self.meson_command + ['setup'] + self.meson_args
        self.mconf_command = self.meson_command + ['configure']
        self.mintro_command = self.meson_command + ['introspect']
        self.wrap_command = self.meson_command + ['wrap']
        self.rewrite_command = self.meson_command + ['rewrite']
        # Backend-specific build commands
        self.build_command, self.clean_command, self.test_command, self.install_command, \
            self.uninstall_command = get_backend_commands(self.backend)
        # Test directories
        self.common_test_dir = os.path.join(src_root, 'test cases/common')
        self.python_test_dir = os.path.join(src_root, 'test cases/python')
        self.rust_test_dir = os.path.join(src_root, 'test cases/rust')
        self.vala_test_dir = os.path.join(src_root, 'test cases/vala')
        self.framework_test_dir = os.path.join(src_root, 'test cases/frameworks')
        self.unit_test_dir = os.path.join(src_root, 'test cases/unit')
        self.rewrite_test_dir = os.path.join(src_root, 'test cases/rewrite')
        self.linuxlike_test_dir = os.path.join(src_root, 'test cases/linuxlike')
        self.objc_test_dir = os.path.join(src_root, 'test cases/objc')
        self.objcpp_test_dir = os.path.join(src_root, 'test cases/objcpp')

        # Misc stuff
        self.orig_env = os.environ.copy()
        if self.backend is Backend.ninja:
            self.no_rebuild_stdout = ['ninja: no work to do.', 'samu: nothing to do']
        else:
            # VS doesn't have a stable output when no changes are done
            # XCode backend is untested with unit tests, help welcome!
            self.no_rebuild_stdout = [f'UNKNOWN BACKEND {self.backend.name!r}']
        os.environ['COLUMNS'] = '80'
        os.environ['PYTHONIOENCODING'] = 'utf8'

        self.builddirs = []
        self.new_builddir()

    def change_builddir(self, newdir):
        self.builddir = newdir
        self.privatedir = os.path.join(self.builddir, 'meson-private')
        self.logdir = os.path.join(self.builddir, 'meson-logs')
        self.installdir = os.path.join(self.builddir, 'install')
        self.distdir = os.path.join(self.builddir, 'meson-dist')
        self.mtest_command = self.meson_command + ['test', '-C', self.builddir]
        self.builddirs.append(self.builddir)

    def new_builddir(self):
        # Keep builddirs inside the source tree so that virus scanners
        # don't complain
        newdir = tempfile.mkdtemp(dir=os.getcwd())
        # In case the directory is inside a symlinked directory, find the real
        # path otherwise we might not find the srcdir from inside the builddir.
        newdir = os.path.realpath(newdir)
        self.change_builddir(newdir)

    def new_builddir_in_tempdir(self):
        # Can't keep the builddir inside the source tree for the umask tests:
        # https://github.com/mesonbuild/meson/pull/5546#issuecomment-509666523
        # And we can't do this for all tests because it causes the path to be
        # a short-path which breaks other tests:
        # https://github.com/mesonbuild/meson/pull/9497
        newdir = tempfile.mkdtemp()
        # In case the directory is inside a symlinked directory, find the real
        # path otherwise we might not find the srcdir from inside the builddir.
        newdir = os.path.realpath(newdir)
        self.change_builddir(newdir)

    def _open_meson_log(self) -> io.TextIOWrapper:
        log = os.path.join(self.logdir, 'meson-log.txt')
        return open(log, encoding='utf-8')

    def _get_meson_log(self) -> T.Optional[str]:
        try:
            with self._open_meson_log() as f:
                return f.read()
        except FileNotFoundError as e:
            print(f"{e.filename!r} doesn't exist", file=sys.stderr)
            return None

    def _print_meson_log(self) -> None:
        log = self._get_meson_log()
        if log:
            print(log)

    def tearDown(self):
        for path in self.builddirs:
            try:
                windows_proof_rmtree(path)
            except FileNotFoundError:
                pass
        os.environ.clear()
        os.environ.update(self.orig_env)
        super().tearDown()

    def _run(self, command, *, workdir=None, override_envvars: T.Optional[T.Mapping[str, str]] = None, stderr=True):
        '''
        Run a command while printing the stdout and stderr to stdout,
        and also return a copy of it
        '''
        # If this call hangs CI will just abort. It is very hard to distinguish
        # between CI issue and test bug in that case. Set timeout and fail loud
        # instead.
        if override_envvars is None:
            env = None
        else:
            env = os.environ.copy()
            env.update(override_envvars)

        proc = subprocess.run(command, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT if stderr else subprocess.PIPE,
                              env=env,
                              encoding='utf-8',
                              text=True, cwd=workdir, timeout=60 * 5)
        print('$', join_args(command))
        print('stdout:')
        print(proc.stdout)
        if not stderr:
            print('stderr:')
            print(proc.stderr)
        if proc.returncode != 0:
            skipped, reason = handle_meson_skip_test(proc.stdout)
            if skipped:
                raise SkipTest(f'Project requested skipping: {reason}')
            raise subprocess.CalledProcessError(proc.returncode, command, output=proc.stdout)
        return proc.stdout

    def init(self, srcdir, *,
             extra_args=None,
             default_args=True,
             inprocess=False,
             override_envvars: T.Optional[T.Mapping[str, str]] = None,
             workdir=None,
             allow_fail: bool = False) -> str:
        """Call `meson setup`

        :param allow_fail: If set to true initialization is allowed to fail.
            When it does the log will be returned instead of stdout.
        :return: the value of stdout on success, or the meson log on failure
            when :param allow_fail: is true
        """
        self.assertPathExists(srcdir)
        if extra_args is None:
            extra_args = []
        if not isinstance(extra_args, list):
            extra_args = [extra_args]
        build_and_src_dir_args = [self.builddir, srcdir]
        args = []
        if default_args:
            args += ['--prefix', self.prefix]
            if self.libdir:
                args += ['--libdir', self.libdir]
            for f in self.meson_native_files:
                args += ['--native-file', f]
            for f in self.meson_cross_files:
                args += ['--cross-file', f]
        self.privatedir = os.path.join(self.builddir, 'meson-private')
        if inprocess:
            try:
                returncode, out, err = run_configure_inprocess(['setup'] + self.meson_args + args + extra_args + build_and_src_dir_args, override_envvars)
            except Exception as e:
                if not allow_fail:
                    self._print_meson_log()
                    raise
                out = self._get_meson_log()  # Best we can do here
                err = ''  # type checkers can't figure out that on this path returncode will always be 0
                returncode = 0
            finally:
                # Close log file to satisfy Windows file locking
                mesonbuild.mlog.shutdown()
                mesonbuild.mlog._logger.log_dir = None
                mesonbuild.mlog._logger.log_file = None

            skipped, reason = handle_meson_skip_test(out)
            if skipped:
                raise SkipTest(f'Project requested skipping: {reason}')
            if returncode != 0:
                self._print_meson_log()
                print('Stdout:\n')
                print(out)
                print('Stderr:\n')
                print(err)
                if not allow_fail:
                    raise RuntimeError('Configure failed')
        else:
            try:
                out = self._run(self.setup_command + args + extra_args + build_and_src_dir_args, override_envvars=override_envvars, workdir=workdir)
            except Exception:
                if not allow_fail:
                    self._print_meson_log()
                    raise
                out = self._get_meson_log()  # best we can do here
        return out

    def build(self, target=None, *, extra_args=None, override_envvars=None, stderr=True):
        if extra_args is None:
            extra_args = []
        # Add arguments for building the target (if specified),
        # and using the build dir (if required, with VS)
        args = get_builddir_target_args(self.backend, self.builddir, target)
        return self._run(self.build_command + args + extra_args, workdir=self.builddir, override_envvars=override_envvars, stderr=stderr)

    def clean(self, *, override_envvars=None):
        dir_args = get_builddir_target_args(self.backend, self.builddir, None)
        self._run(self.clean_command + dir_args, workdir=self.builddir, override_envvars=override_envvars)

    def run_tests(self, *, inprocess=False, override_envvars=None):
        if not inprocess:
            return self._run(self.test_command, workdir=self.builddir, override_envvars=override_envvars)
        else:
            with mock.patch.dict(os.environ, override_envvars):
                return run_mtest_inprocess(['-C', self.builddir])[1]

    def install(self, *, use_destdir=True, override_envvars=None):
        if self.backend is not Backend.ninja:
            raise SkipTest(f'{self.backend.name!r} backend can\'t install files')
        if use_destdir:
            destdir = {'DESTDIR': self.installdir}
            if override_envvars is None:
                override_envvars = destdir
            else:
                override_envvars.update(destdir)
        return self._run(self.install_command, workdir=self.builddir, override_envvars=override_envvars)

    def uninstall(self, *, override_envvars=None):
        self._run(self.uninstall_command, workdir=self.builddir, override_envvars=override_envvars)

    def run_target(self, target, *, override_envvars=None):
        '''
        Run a Ninja target while printing the stdout and stderr to stdout,
        and also return a copy of it
        '''
        return self.build(target=target, override_envvars=override_envvars)

    def setconf(self, arg: T.Sequence[str], will_build: bool = True) -> None:
        if isinstance(arg, str):
            arg = [arg]
        else:
            arg = list(arg)
        if will_build:
            ensure_backend_detects_changes(self.backend)
        self._run(self.mconf_command + arg + [self.builddir])

    def getconf(self, optname: str):
        opts = self.introspect('--buildoptions')
        for x in opts:
            if x.get('name') == optname:
                return x.get('value')
        self.fail(f'Option {optname} not found')

    def wipe(self):
        windows_proof_rmtree(self.builddir)

    def utime(self, f):
        ensure_backend_detects_changes(self.backend)
        os.utime(f)

    def get_compdb(self):
        if self.backend is not Backend.ninja:
            raise SkipTest(f'Compiler db not available with {self.backend.name} backend')
        try:
            with open(os.path.join(self.builddir, 'compile_commands.json'), encoding='utf-8') as ifile:
                contents = json.load(ifile)
        except FileNotFoundError:
            raise SkipTest('Compiler db not found')
        # If Ninja is using .rsp files, generate them, read their contents, and
        # replace it as the command for all compile commands in the parsed json.
        if len(contents) > 0 and contents[0]['command'].endswith('.rsp'):
            # Pretend to build so that the rsp files are generated
            self.build(extra_args=['-d', 'keeprsp', '-n'])
            for each in contents:
                # Extract the actual command from the rsp file
                compiler, rsp = each['command'].split(' @')
                rsp = os.path.join(self.builddir, rsp)
                # Replace the command with its contents
                with open(rsp, encoding='utf-8') as f:
                    each['command'] = compiler + ' ' + f.read()
        return contents

    def get_meson_log_raw(self):
        with self._open_meson_log() as f:
            return f.read()

    def get_meson_log(self):
        with self._open_meson_log() as f:
            return f.readlines()

    def get_meson_log_compiler_checks(self):
        '''
        Fetch a list command-lines run by meson for compiler checks.
        Each command-line is returned as a list of arguments.
        '''
        prefix = 'Command line: `'
        suffix = '` -> 0\n'
        with self._open_meson_log() as log:
            cmds = [split_args(l[len(prefix):-len(suffix)]) for l in log if l.startswith(prefix)]
            return cmds

    def get_meson_log_sanitychecks(self):
        '''
        Same as above, but for the sanity checks that were run
        '''
        prefix = 'Sanity check compiler command line:'
        with self._open_meson_log() as log:
            cmds = [l[len(prefix):].split() for l in log if l.startswith(prefix)]
            return cmds

    def introspect(self, args):
        if isinstance(args, str):
            args = [args]
        out = subprocess.check_output(self.mintro_command + args + [self.builddir],
                                      universal_newlines=True)
        return json.loads(out)

    def introspect_directory(self, directory, args):
        if isinstance(args, str):
            args = [args]
        out = subprocess.check_output(self.mintro_command + args + [directory],
                                      universal_newlines=True)
        try:
            obj = json.loads(out)
        except Exception as e:
            print(out)
            raise e
        return obj

    def assertPathEqual(self, path1, path2):
        '''
        Handles a lot of platform-specific quirks related to paths such as
        separator, case-sensitivity, etc.
        '''
        self.assertEqual(PurePath(path1), PurePath(path2))

    def assertPathListEqual(self, pathlist1, pathlist2):
        self.assertEqual(len(pathlist1), len(pathlist2))
        worklist = list(zip(pathlist1, pathlist2))
        for i in worklist:
            if i[0] is None:
                self.assertEqual(i[0], i[1])
            else:
                self.assertPathEqual(i[0], i[1])

    def assertPathBasenameEqual(self, path, basename):
        msg = f'{path!r} does not end with {basename!r}'
        # We cannot use os.path.basename because it returns '' when the path
        # ends with '/' for some silly reason. This is not how the UNIX utility
        # `basename` works.
        path_basename = PurePath(path).parts[-1]
        self.assertEqual(PurePath(path_basename), PurePath(basename), msg)

    def assertReconfiguredBuildIsNoop(self):
        'Assert that we reconfigured and then there was nothing to do'
        ret = self.build(stderr=False)
        self.assertIn('The Meson build system', ret)
        if self.backend is Backend.ninja:
            for line in ret.split('\n'):
                if line in self.no_rebuild_stdout:
                    break
            else:
                raise AssertionError('build was reconfigured, but was not no-op')
        elif self.backend is Backend.vs:
            # Ensure that some target said that no rebuild was done
            # XXX: Note CustomBuild did indeed rebuild, because of the regen checker!
            self.assertIn('ClCompile:\n  All outputs are up-to-date.', ret)
            self.assertIn('Link:\n  All outputs are up-to-date.', ret)
            # Ensure that no targets were built
            self.assertNotRegex(ret, re.compile('ClCompile:\n [^\n]*cl', flags=re.IGNORECASE))
            self.assertNotRegex(ret, re.compile('Link:\n [^\n]*link', flags=re.IGNORECASE))
        elif self.backend is Backend.xcode:
            raise SkipTest('Please help us fix this test on the xcode backend')
        else:
            raise RuntimeError(f'Invalid backend: {self.backend.name!r}')

    def assertBuildIsNoop(self):
        ret = self.build(stderr=False)
        if self.backend is Backend.ninja:
            self.assertIn(ret.split('\n')[-2], self.no_rebuild_stdout)
        elif self.backend is Backend.vs:
            # Ensure that some target of each type said that no rebuild was done
            # We always have at least one CustomBuild target for the regen checker
            self.assertIn('CustomBuild:\n  All outputs are up-to-date.', ret)
            self.assertIn('ClCompile:\n  All outputs are up-to-date.', ret)
            self.assertIn('Link:\n  All outputs are up-to-date.', ret)
            # Ensure that no targets were built
            self.assertNotRegex(ret, re.compile('CustomBuild:\n [^\n]*cl', flags=re.IGNORECASE))
            self.assertNotRegex(ret, re.compile('ClCompile:\n [^\n]*cl', flags=re.IGNORECASE))
            self.assertNotRegex(ret, re.compile('Link:\n [^\n]*link', flags=re.IGNORECASE))
        elif self.backend is Backend.xcode:
            raise SkipTest('Please help us fix this test on the xcode backend')
        else:
            raise RuntimeError(f'Invalid backend: {self.backend.name!r}')

    def assertRebuiltTarget(self, target):
        ret = self.build()
        if self.backend is Backend.ninja:
            self.assertIn(f'Linking target {target}', ret)
        elif self.backend is Backend.vs:
            # Ensure that this target was rebuilt
            linkre = re.compile('Link:\n [^\n]*link[^\n]*' + target, flags=re.IGNORECASE)
            self.assertRegex(ret, linkre)
        elif self.backend is Backend.xcode:
            raise SkipTest('Please help us fix this test on the xcode backend')
        else:
            raise RuntimeError(f'Invalid backend: {self.backend.name!r}')

    @staticmethod
    def get_target_from_filename(filename):
        base = os.path.splitext(filename)[0]
        if base.startswith(('lib', 'cyg')):
            return base[3:]
        return base

    def assertBuildRelinkedOnlyTarget(self, target):
        ret = self.build()
        if self.backend is Backend.ninja:
            linked_targets = []
            for line in ret.split('\n'):
                if 'Linking target' in line:
                    fname = line.rsplit('target ')[-1]
                    linked_targets.append(self.get_target_from_filename(fname))
            self.assertEqual(linked_targets, [target])
        elif self.backend is Backend.vs:
            # Ensure that this target was rebuilt
            linkre = re.compile(r'Link:\n  [^\n]*link.exe[^\n]*/OUT:".\\([^"]*)"', flags=re.IGNORECASE)
            matches = linkre.findall(ret)
            self.assertEqual(len(matches), 1, msg=matches)
            self.assertEqual(self.get_target_from_filename(matches[0]), target)
        elif self.backend is Backend.xcode:
            raise SkipTest('Please help us fix this test on the xcode backend')
        else:
            raise RuntimeError(f'Invalid backend: {self.backend.name!r}')

    def assertPathExists(self, path):
        m = f'Path {path!r} should exist'
        self.assertTrue(os.path.exists(path), msg=m)

    def assertPathDoesNotExist(self, path):
        m = f'Path {path!r} should not exist'
        self.assertFalse(os.path.exists(path), msg=m)

    def assertLength(self, val, length):
        assert len(val) == length, f'{val} is not length {length}'

    def copy_srcdir(self, srcdir: str) -> str:
        """Copies a source tree and returns that copy.

        ensures that the copied tree is deleted after running.

        :param srcdir: The locaiton of the source tree to copy
        :return: The location of the copy
        """
        dest = tempfile.mkdtemp()
        self.addCleanup(windows_proof_rmtree, dest)

        # shutil.copytree expects the destinatin directory to not exist, Once
        # python 3.8 is required the `dirs_exist_ok` parameter negates the need
        # for this
        dest = os.path.join(dest, 'subdir')

        shutil.copytree(srcdir, dest)

        return dest
