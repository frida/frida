# SPDX-License-Identifier: Apache-2.0
# Copyright 2016-2021 The Meson development team

import subprocess
import re
import json
import tempfile
import textwrap
import os
import shutil
import platform
import pickle
import zipfile, tarfile
import sys
from unittest import mock, SkipTest, skipIf, skipUnless
from contextlib import contextmanager
from glob import glob
from pathlib import (PurePath, Path)
import typing as T

import mesonbuild.mlog
import mesonbuild.depfile
import mesonbuild.dependencies.base
import mesonbuild.dependencies.factory
import mesonbuild.envconfig
import mesonbuild.environment
import mesonbuild.coredata
import mesonbuild.modules.gnome
from mesonbuild.mesonlib import (
    BuildDirLock, MachineChoice, is_windows, is_osx, is_cygwin, is_dragonflybsd,
    is_sunos, windows_proof_rmtree, python_command, version_compare, split_args, quote_arg,
    relpath, is_linux, git, search_version, do_conf_file, do_conf_str, default_prefix,
    MesonException, EnvironmentException, OptionKey,
    windows_proof_rm
)
from mesonbuild.programs import ExternalProgram

from mesonbuild.compilers.mixins.clang import ClangCompiler
from mesonbuild.compilers.mixins.gnu import GnuCompiler
from mesonbuild.compilers.mixins.intel import IntelGnuLikeCompiler
from mesonbuild.compilers.c import VisualStudioCCompiler, ClangClCCompiler
from mesonbuild.compilers.cpp import VisualStudioCPPCompiler, ClangClCPPCompiler
from mesonbuild.compilers import (
    detect_static_linker, detect_c_compiler, compiler_from_language,
    detect_compiler_for
)
from mesonbuild.linkers import linkers

from mesonbuild.dependencies.pkgconfig import PkgConfigDependency
from mesonbuild.build import Target, ConfigurationData, Executable, SharedLibrary, StaticLibrary
from mesonbuild import mtest
import mesonbuild.modules.pkgconfig
from mesonbuild.scripts import destdir_join

from mesonbuild.wrap.wrap import PackageDefinition, WrapException

from run_tests import (
    Backend, exe_suffix, get_fake_env, get_convincing_fake_env_and_cc
)

from .baseplatformtests import BasePlatformTests
from .helpers import *

@contextmanager
def temp_filename():
    '''A context manager which provides a filename to an empty temporary file.

    On exit the file will be deleted.
    '''

    fd, filename = tempfile.mkstemp()
    os.close(fd)
    try:
        yield filename
    finally:
        try:
            os.remove(filename)
        except OSError:
            pass

def git_init(project_dir):
    # If a user has git configuration init.defaultBranch set we want to override that
    with tempfile.TemporaryDirectory() as d:
        out = git(['--version'], str(d))[1]
    if version_compare(search_version(out), '>= 2.28'):
        extra_cmd = ['--initial-branch', 'master']
    else:
        extra_cmd = []

    subprocess.check_call(['git', 'init'] + extra_cmd, cwd=project_dir, stdout=subprocess.DEVNULL)
    subprocess.check_call(['git', 'config',
                           'user.name', 'Author Person'], cwd=project_dir)
    subprocess.check_call(['git', 'config',
                           'user.email', 'teh_coderz@example.com'], cwd=project_dir)
    _git_add_all(project_dir)

def _git_add_all(project_dir):
    subprocess.check_call('git add *', cwd=project_dir, shell=True,
                          stdout=subprocess.DEVNULL)
    subprocess.check_call(['git', 'commit', '--no-gpg-sign', '-a', '-m', 'I am a project'], cwd=project_dir,
                          stdout=subprocess.DEVNULL)

class AllPlatformTests(BasePlatformTests):
    '''
    Tests that should run on all platforms
    '''

    def test_default_options_prefix(self):
        '''
        Tests that setting a prefix in default_options in project() works.
        Can't be an ordinary test because we pass --prefix to meson there.
        https://github.com/mesonbuild/meson/issues/1349
        '''
        testdir = os.path.join(self.common_test_dir, '87 default options')
        self.init(testdir, default_args=False, inprocess=True)
        opts = self.introspect('--buildoptions')
        for opt in opts:
            if opt['name'] == 'prefix':
                prefix = opt['value']
                break
        else:
            raise self.fail('Did not find option "prefix"')
        self.assertEqual(prefix, '/absoluteprefix')

    def test_do_conf_file_preserve_newlines(self):

        def conf_file(in_data, confdata):
            with temp_filename() as fin:
                with open(fin, 'wb') as fobj:
                    fobj.write(in_data.encode('utf-8'))
                with temp_filename() as fout:
                    do_conf_file(fin, fout, confdata, 'meson')
                    with open(fout, 'rb') as fobj:
                        return fobj.read().decode('utf-8')

        confdata = {'VAR': ('foo', 'bar')}
        self.assertEqual(conf_file('@VAR@\n@VAR@\n', confdata), 'foo\nfoo\n')
        self.assertEqual(conf_file('@VAR@\r\n@VAR@\r\n', confdata), 'foo\r\nfoo\r\n')

    def test_do_conf_file_by_format(self):
        def conf_str(in_data, confdata, vformat):
            (result, missing_variables, confdata_useless) = do_conf_str('configuration_file', in_data, confdata, variable_format = vformat)
            return '\n'.join(result)

        def check_meson_format(confdata, result):
            self.assertEqual(conf_str(['#mesondefine VAR'], confdata, 'meson'), result)

        def check_cmake_format_simple(confdata, result):
            self.assertEqual(conf_str(['#cmakedefine VAR'], confdata, 'cmake'), result)

        def check_cmake_formats_full(confdata, result):
            self.assertEqual(conf_str(['#cmakedefine VAR ${VAR}'], confdata, 'cmake'), result)
            self.assertEqual(conf_str(['#cmakedefine VAR @VAR@'], confdata, 'cmake@'), result)

        def check_formats(confdata, result):
            check_meson_format(confdata, result)
            check_cmake_formats_full(confdata, result)

        confdata = ConfigurationData()
        # Key error as they do not exists
        check_formats(confdata, '/* #undef VAR */\n')

        # Check boolean
        confdata.values = {'VAR': (False, 'description')}
        check_meson_format(confdata, '#undef VAR\n')
        check_cmake_formats_full(confdata, '/* #undef VAR */\n')

        confdata.values = {'VAR': (True, 'description')}
        check_meson_format(confdata, '#define VAR\n')
        check_cmake_format_simple(confdata, '#define VAR\n')
        check_cmake_formats_full(confdata, '#define VAR 1\n')

        # Check string
        confdata.values = {'VAR': ('value', 'description')}
        check_formats(confdata, '#define VAR value\n')

        # Check integer
        confdata.values = {'VAR': (10, 'description')}
        check_formats(confdata, '#define VAR 10\n')

        # Checking if cmakedefine behaves as it does with cmake
        confdata.values = {'VAR': ("var", 'description')}
        self.assertEqual(conf_str(['#cmakedefine VAR @VAR@'], confdata, 'cmake@'), '#define VAR var\n')

        confdata.values = {'VAR': (True, 'description')}
        self.assertEqual(conf_str(['#cmakedefine01 VAR'], confdata, 'cmake'), '#define VAR 1\n')

        confdata.values = {'VAR': (0, 'description')}
        self.assertEqual(conf_str(['#cmakedefine01 VAR'], confdata, 'cmake'), '#define VAR 0\n')
        confdata.values = {'VAR': (False, 'description')}
        self.assertEqual(conf_str(['#cmakedefine01 VAR'], confdata, 'cmake'), '#define VAR 0\n')

        confdata.values = {}
        self.assertEqual(conf_str(['#cmakedefine01 VAR'], confdata, 'cmake'), '#define VAR 0\n')
        self.assertEqual(conf_str(['#cmakedefine VAR @VAR@'], confdata, 'cmake@'), '/* #undef VAR */\n')

        confdata.values = {'VAR': (5, 'description')}
        self.assertEqual(conf_str(['#cmakedefine VAR'], confdata, 'cmake'), '#define VAR\n')

        # Check multiple string with cmake formats
        confdata.values = {'VAR': ('value', 'description')}
        self.assertEqual(conf_str(['#cmakedefine VAR xxx @VAR@ yyy @VAR@'], confdata, 'cmake@'), '#define VAR xxx value yyy value\n')
        self.assertEqual(conf_str(['#define VAR xxx @VAR@ yyy @VAR@'], confdata, 'cmake@'), '#define VAR xxx value yyy value')
        self.assertEqual(conf_str(['#cmakedefine VAR xxx ${VAR} yyy ${VAR}'], confdata, 'cmake'), '#define VAR xxx value yyy value\n')
        self.assertEqual(conf_str(['#define VAR xxx ${VAR} yyy ${VAR}'], confdata, 'cmake'), '#define VAR xxx value yyy value')

        # Handles meson format exceptions
        #   Unknown format
        self.assertRaises(MesonException, conf_str, ['#mesondefine VAR xxx'], confdata, 'unknown_format')
        #   More than 2 params in mesondefine
        self.assertRaises(MesonException, conf_str, ['#mesondefine VAR xxx'], confdata, 'meson')
        #   Mismatched line with format
        self.assertRaises(MesonException, conf_str, ['#cmakedefine VAR'], confdata, 'meson')
        self.assertRaises(MesonException, conf_str, ['#mesondefine VAR'], confdata, 'cmake')
        self.assertRaises(MesonException, conf_str, ['#mesondefine VAR'], confdata, 'cmake@')
        #   Dict value in confdata
        confdata.values = {'VAR': (['value'], 'description')}
        self.assertRaises(MesonException, conf_str, ['#mesondefine VAR'], confdata, 'meson')

    def test_absolute_prefix_libdir(self):
        '''
        Tests that setting absolute paths for --prefix and --libdir work. Can't
        be an ordinary test because these are set via the command-line.
        https://github.com/mesonbuild/meson/issues/1341
        https://github.com/mesonbuild/meson/issues/1345
        '''
        testdir = os.path.join(self.common_test_dir, '87 default options')
        # on Windows, /someabs is *not* an absolute path
        prefix = 'x:/someabs' if is_windows() else '/someabs'
        libdir = 'libdir'
        extra_args = ['--prefix=' + prefix,
                      # This can just be a relative path, but we want to test
                      # that passing this as an absolute path also works
                      '--libdir=' + prefix + '/' + libdir]
        self.init(testdir, extra_args=extra_args, default_args=False)
        opts = self.introspect('--buildoptions')
        for opt in opts:
            if opt['name'] == 'prefix':
                self.assertEqual(prefix, opt['value'])
            elif opt['name'] == 'libdir':
                self.assertEqual(libdir, opt['value'])

    def test_libdir_can_be_outside_prefix(self):
        '''
        Tests that libdir is allowed to be outside prefix.
        Must be a unit test for obvious reasons.
        '''
        testdir = os.path.join(self.common_test_dir, '1 trivial')
        # libdir being inside prefix is ok
        if is_windows():
            args = ['--prefix', 'x:/opt', '--libdir', 'x:/opt/lib32']
        else:
            args = ['--prefix', '/opt', '--libdir', '/opt/lib32']
        self.init(testdir, extra_args=args)
        self.wipe()
        # libdir not being inside prefix is ok too
        if is_windows():
            args = ['--prefix', 'x:/usr', '--libdir', 'x:/opt/lib32']
        else:
            args = ['--prefix', '/usr', '--libdir', '/opt/lib32']
        self.init(testdir, extra_args=args)
        self.wipe()
        # libdir can be outside prefix even when set via mesonconf
        self.init(testdir)
        if is_windows():
            self.setconf('-Dlibdir=x:/opt', will_build=False)
        else:
            self.setconf('-Dlibdir=/opt', will_build=False)

    def test_prefix_dependent_defaults(self):
        '''
        Tests that configured directory paths are set to prefix dependent
        defaults.
        '''
        testdir = os.path.join(self.common_test_dir, '1 trivial')
        expected = {
            '/opt': {'prefix': '/opt',
                     'bindir': 'bin', 'datadir': 'share', 'includedir': 'include',
                     'infodir': 'share/info',
                     'libexecdir': 'libexec', 'localedir': 'share/locale',
                     'localstatedir': 'var', 'mandir': 'share/man',
                     'sbindir': 'sbin', 'sharedstatedir': 'com',
                     'sysconfdir': 'etc'},
            '/usr': {'prefix': '/usr',
                     'bindir': 'bin', 'datadir': 'share', 'includedir': 'include',
                     'infodir': 'share/info',
                     'libexecdir': 'libexec', 'localedir': 'share/locale',
                     'localstatedir': '/var', 'mandir': 'share/man',
                     'sbindir': 'sbin', 'sharedstatedir': '/var/lib',
                     'sysconfdir': '/etc'},
            '/usr/local': {'prefix': '/usr/local',
                           'bindir': 'bin', 'datadir': 'share',
                           'includedir': 'include', 'infodir': 'share/info',
                           'libexecdir': 'libexec',
                           'localedir': 'share/locale',
                           'localstatedir': '/var/local', 'mandir': 'share/man',
                           'sbindir': 'sbin', 'sharedstatedir': '/var/local/lib',
                           'sysconfdir': 'etc'},
            # N.B. We don't check 'libdir' as it's platform dependent, see
            # default_libdir():
        }

        if default_prefix() == '/usr/local':
            expected[None] = expected['/usr/local']

        for prefix in expected:
            args = []
            if prefix:
                args += ['--prefix', prefix]
            self.init(testdir, extra_args=args, default_args=False)
            opts = self.introspect('--buildoptions')
            for opt in opts:
                name = opt['name']
                value = opt['value']
                if name in expected[prefix]:
                    self.assertEqual(value, expected[prefix][name])
            self.wipe()

    def test_default_options_prefix_dependent_defaults(self):
        '''
        Tests that setting a prefix in default_options in project() sets prefix
        dependent defaults for other options, and that those defaults can
        be overridden in default_options or by the command line.
        '''
        testdir = os.path.join(self.common_test_dir, '163 default options prefix dependent defaults')
        expected = {
            '':
            {'prefix':         '/usr',
             'sysconfdir':     '/etc',
             'localstatedir':  '/var',
             'sharedstatedir': '/sharedstate'},
            '--prefix=/usr':
            {'prefix':         '/usr',
             'sysconfdir':     '/etc',
             'localstatedir':  '/var',
             'sharedstatedir': '/sharedstate'},
            '--sharedstatedir=/var/state':
            {'prefix':         '/usr',
             'sysconfdir':     '/etc',
             'localstatedir':  '/var',
             'sharedstatedir': '/var/state'},
            '--sharedstatedir=/var/state --prefix=/usr --sysconfdir=sysconf':
            {'prefix':         '/usr',
             'sysconfdir':     'sysconf',
             'localstatedir':  '/var',
             'sharedstatedir': '/var/state'},
        }
        for args in expected:
            self.init(testdir, extra_args=args.split(), default_args=False)
            opts = self.introspect('--buildoptions')
            for opt in opts:
                name = opt['name']
                value = opt['value']
                if name in expected[args]:
                    self.assertEqual(value, expected[args][name])
            self.wipe()

    def test_clike_get_library_dirs(self):
        env = get_fake_env()
        cc = detect_c_compiler(env, MachineChoice.HOST)
        for d in cc.get_library_dirs(env):
            self.assertTrue(os.path.exists(d))
            self.assertTrue(os.path.isdir(d))
            self.assertTrue(os.path.isabs(d))

    def test_static_library_overwrite(self):
        '''
        Tests that static libraries are never appended to, always overwritten.
        Has to be a unit test because this involves building a project,
        reconfiguring, and building it again so that `ar` is run twice on the
        same static library.
        https://github.com/mesonbuild/meson/issues/1355
        '''
        testdir = os.path.join(self.common_test_dir, '3 static')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        static_linker = detect_static_linker(env, cc)
        if is_windows():
            raise SkipTest('https://github.com/mesonbuild/meson/issues/1526')
        if not isinstance(static_linker, linkers.ArLinker):
            raise SkipTest('static linker is not `ar`')
        # Configure
        self.init(testdir)
        # Get name of static library
        targets = self.introspect('--targets')
        self.assertGreaterEqual(len(targets), 1)
        libname = targets[0]['filename'][0]
        # Build and get contents of static library
        self.build()
        before = self._run(['ar', 't', os.path.join(self.builddir, libname)]).split()
        # Filter out non-object-file contents
        before = [f for f in before if f.endswith(('.o', '.obj'))]
        # Static library should contain only one object
        self.assertEqual(len(before), 1, msg=before)
        # Change the source to be built into the static library
        self.setconf('-Dsource=libfile2.c')
        self.build()
        after = self._run(['ar', 't', os.path.join(self.builddir, libname)]).split()
        # Filter out non-object-file contents
        after = [f for f in after if f.endswith(('.o', '.obj'))]
        # Static library should contain only one object
        self.assertEqual(len(after), 1, msg=after)
        # and the object must have changed
        self.assertNotEqual(before, after)

    def test_static_compile_order(self):
        '''
        Test that the order of files in a compiler command-line while compiling
        and linking statically is deterministic. This can't be an ordinary test
        case because we need to inspect the compiler database.
        https://github.com/mesonbuild/meson/pull/951
        '''
        testdir = os.path.join(self.common_test_dir, '5 linkstatic')
        self.init(testdir)
        compdb = self.get_compdb()
        # Rules will get written out in this order
        self.assertTrue(compdb[0]['file'].endswith("libfile.c"))
        self.assertTrue(compdb[1]['file'].endswith("libfile2.c"))
        self.assertTrue(compdb[2]['file'].endswith("libfile3.c"))
        self.assertTrue(compdb[3]['file'].endswith("libfile4.c"))
        # FIXME: We don't have access to the linker command

    def test_replace_unencodable_xml_chars(self):
        '''
        Test that unencodable xml chars are replaced with their
        printable representation
        https://github.com/mesonbuild/meson/issues/9894
        '''
        # Create base string(\nHello Meson\n) to see valid chars are not replaced
        base_string_invalid = '\n\x48\x65\x6c\x6c\x6f\x20\x4d\x65\x73\x6f\x6e\n'
        base_string_valid = '\nHello Meson\n'
        # Create invalid input from all known unencodable chars
        invalid_string = (
            '\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0b\x0c\x0e\x0f\x10\x11'
            '\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f\x7f'
            '\x80\x81\x82\x83\x84\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f'
            '\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e'
            '\x9f\ufdd0\ufdd1\ufdd2\ufdd3\ufdd4\ufdd5\ufdd6\ufdd7\ufdd8'
            '\ufdd9\ufdda\ufddb\ufddc\ufddd\ufdde\ufddf\ufde0\ufde1'
            '\ufde2\ufde3\ufde4\ufde5\ufde6\ufde7\ufde8\ufde9\ufdea'
            '\ufdeb\ufdec\ufded\ufdee\ufdef\ufffe\uffff')
        if sys.maxunicode >= 0x10000:
            invalid_string = invalid_string + (
                '\U0001fffe\U0001ffff\U0002fffe\U0002ffff'
                '\U0003fffe\U0003ffff\U0004fffe\U0004ffff'
                '\U0005fffe\U0005ffff\U0006fffe\U0006ffff'
                '\U0007fffe\U0007ffff\U0008fffe\U0008ffff'
                '\U0009fffe\U0009ffff\U000afffe\U000affff'
                '\U000bfffe\U000bffff\U000cfffe\U000cffff'
                '\U000dfffe\U000dffff\U000efffe\U000effff'
                '\U000ffffe\U000fffff\U0010fffe\U0010ffff')

        valid_string = base_string_valid + repr(invalid_string)[1:-1] + base_string_valid
        invalid_string = base_string_invalid + invalid_string + base_string_invalid
        fixed_string = mtest.replace_unencodable_xml_chars(invalid_string)
        self.assertEqual(fixed_string, valid_string)

    def test_replace_unencodable_xml_chars_unit(self):
        '''
        Test that unencodable xml chars are replaced with their
        printable representation
        https://github.com/mesonbuild/meson/issues/9894
        '''
        if not shutil.which('xmllint'):
            raise SkipTest('xmllint not installed')
        testdir = os.path.join(self.unit_test_dir, '111 replace unencodable xml chars')
        self.init(testdir)
        tests_command_output = self.run_tests()
        junit_xml_logs = Path(self.logdir, 'testlog.junit.xml')
        subprocess.run(['xmllint', junit_xml_logs], check=True)
        # Ensure command output and JSON / text logs are not mangled.
        raw_output_sample = '\x00\x01\x02\x03\x04\x05\x06\x07\x08\x0b'
        assert raw_output_sample in tests_command_output
        text_log = Path(self.logdir, 'testlog.txt').read_text(encoding='utf-8')
        assert raw_output_sample in text_log
        json_log = json.loads(Path(self.logdir, 'testlog.json').read_bytes())
        assert raw_output_sample in json_log['stdout']

    def test_run_target_files_path(self):
        '''
        Test that run_targets are run from the correct directory
        https://github.com/mesonbuild/meson/issues/957
        '''
        testdir = os.path.join(self.common_test_dir, '51 run target')
        self.init(testdir)
        self.run_target('check_exists')
        self.run_target('check-env')
        self.run_target('check-env-ct')

    def test_run_target_subdir(self):
        '''
        Test that run_targets are run from the correct directory
        https://github.com/mesonbuild/meson/issues/957
        '''
        testdir = os.path.join(self.common_test_dir, '51 run target')
        self.init(testdir)
        self.run_target('textprinter')

    def test_install_introspection(self):
        '''
        Tests that the Meson introspection API exposes install filenames correctly
        https://github.com/mesonbuild/meson/issues/829
        '''
        if self.backend is not Backend.ninja:
            raise SkipTest(f'{self.backend.name!r} backend can\'t install files')
        testdir = os.path.join(self.common_test_dir, '8 install')
        self.init(testdir)
        intro = self.introspect('--targets')
        if intro[0]['type'] == 'executable':
            intro = intro[::-1]
        self.assertPathListEqual(intro[0]['install_filename'], ['/usr/lib/libstat.a'])
        self.assertPathListEqual(intro[1]['install_filename'], ['/usr/bin/prog' + exe_suffix])

    def test_install_subdir_introspection(self):
        '''
        Test that the Meson introspection API also contains subdir install information
        https://github.com/mesonbuild/meson/issues/5556
        '''
        testdir = os.path.join(self.common_test_dir, '59 install subdir')
        self.init(testdir)
        intro = self.introspect('--installed')
        expected = {
            'nested_elided/sub': 'share',
            'new_directory': 'share/new_directory',
            'sub/sub1': 'share/sub1',
            'sub1': 'share/sub1',
            'sub2': 'share/sub2',
            'sub3': '/usr/share/sub3',
            'sub_elided': 'share',
            'subdir/sub1': 'share/sub1',
            'subdir/sub_elided': 'share',
        }

        self.assertEqual(len(intro), len(expected))

        # Convert expected to PurePath
        expected_converted = {PurePath(os.path.join(testdir, key)): PurePath(os.path.join(self.prefix, val)) for key, val in expected.items()}
        intro_converted = {PurePath(key): PurePath(val) for key, val in intro.items()}

        for src, dst in expected_converted.items():
            self.assertIn(src, intro_converted)
            self.assertEqual(dst, intro_converted[src])

    def test_install_introspection_multiple_outputs(self):
        '''
        Tests that the Meson introspection API exposes multiple install filenames correctly without crashing
        https://github.com/mesonbuild/meson/pull/4555

        Reverted to the first file only because of https://github.com/mesonbuild/meson/pull/4547#discussion_r244173438
        TODO Change the format to a list officially in a followup PR
        '''
        if self.backend is not Backend.ninja:
            raise SkipTest(f'{self.backend.name!r} backend can\'t install files')
        testdir = os.path.join(self.common_test_dir, '140 custom target multiple outputs')
        self.init(testdir)
        intro = self.introspect('--targets')
        if intro[0]['type'] == 'executable':
            intro = intro[::-1]
        self.assertPathListEqual(intro[0]['install_filename'], ['/usr/include/diff.h', '/usr/bin/diff.sh'])
        self.assertPathListEqual(intro[1]['install_filename'], ['/opt/same.h', '/opt/same.sh'])
        self.assertPathListEqual(intro[2]['install_filename'], ['/usr/include/first.h', None])
        self.assertPathListEqual(intro[3]['install_filename'], [None, '/usr/bin/second.sh'])

    def read_install_logs(self):
        # Find logged files and directories
        with Path(self.builddir, 'meson-logs', 'install-log.txt').open(encoding='utf-8') as f:
            return list(map(lambda l: Path(l.strip()),
                              filter(lambda l: not l.startswith('#'),
                                     f.readlines())))

    def test_install_log_content(self):
        '''
        Tests that the install-log.txt is consistent with the installed files and directories.
        Specifically checks that the log file only contains one entry per file/directory.
        https://github.com/mesonbuild/meson/issues/4499
        '''
        testdir = os.path.join(self.common_test_dir, '59 install subdir')
        self.init(testdir)
        self.install()
        installpath = Path(self.installdir)
        # Find installed files and directories
        expected = {installpath: 0}
        for name in installpath.rglob('*'):
            expected[name] = 0
        logged = self.read_install_logs()
        for name in logged:
            self.assertTrue(name in expected, f'Log contains extra entry {name}')
            expected[name] += 1

        for name, count in expected.items():
            self.assertGreater(count, 0, f'Log is missing entry for {name}')
            self.assertLess(count, 2, f'Log has multiple entries for {name}')

        # Verify that with --dry-run we obtain the same logs but with nothing
        # actually installed
        windows_proof_rmtree(self.installdir)
        self._run(self.meson_command + ['install', '--dry-run', '--destdir', self.installdir], workdir=self.builddir)
        self.assertEqual(logged, self.read_install_logs())
        self.assertFalse(os.path.exists(self.installdir))

        # If destdir is relative to build directory it should install
        # exactly the same files.
        rel_installpath = os.path.relpath(self.installdir, self.builddir)
        self._run(self.meson_command + ['install', '--dry-run', '--destdir', rel_installpath, '-C', self.builddir])
        self.assertEqual(logged, self.read_install_logs())

    def test_uninstall(self):
        exename = os.path.join(self.installdir, 'usr/bin/prog' + exe_suffix)
        dirname = os.path.join(self.installdir, 'usr/share/dir')
        testdir = os.path.join(self.common_test_dir, '8 install')
        self.init(testdir)
        self.assertPathDoesNotExist(exename)
        self.install()
        self.assertPathExists(exename)
        self.uninstall()
        self.assertPathDoesNotExist(exename)
        self.assertPathDoesNotExist(dirname)

    def test_forcefallback(self):
        testdir = os.path.join(self.unit_test_dir, '31 forcefallback')
        self.init(testdir, extra_args=['--wrap-mode=forcefallback'])
        self.build()
        self.run_tests()

    def test_implicit_forcefallback(self):
        testdir = os.path.join(self.unit_test_dir, '96 implicit force fallback')
        with self.assertRaises(subprocess.CalledProcessError):
            self.init(testdir)
        self.init(testdir, extra_args=['--wrap-mode=forcefallback'])
        self.new_builddir()
        self.init(testdir, extra_args=['--force-fallback-for=something'])

    def test_nopromote(self):
        testdir = os.path.join(self.common_test_dir, '98 subproject subdir')
        with self.assertRaises(subprocess.CalledProcessError) as cm:
            self.init(testdir, extra_args=['--wrap-mode=nopromote'])
        self.assertIn('dependency subsub found: NO', cm.exception.stdout)

    def test_force_fallback_for(self):
        testdir = os.path.join(self.unit_test_dir, '31 forcefallback')
        self.init(testdir, extra_args=['--force-fallback-for=zlib,foo'])
        self.build()
        self.run_tests()

    def test_force_fallback_for_nofallback(self):
        testdir = os.path.join(self.unit_test_dir, '31 forcefallback')
        self.init(testdir, extra_args=['--force-fallback-for=zlib,foo', '--wrap-mode=nofallback'])
        self.build()
        self.run_tests()

    def test_testrepeat(self):
        testdir = os.path.join(self.common_test_dir, '206 tap tests')
        self.init(testdir)
        self.build()
        self._run(self.mtest_command + ['--repeat=2'])

    def test_verbose(self):
        testdir = os.path.join(self.common_test_dir, '206 tap tests')
        self.init(testdir)
        self.build()
        out = self._run(self.mtest_command + ['--suite', 'verbose'])
        self.assertIn('1/1 subtest 1', out)

    def test_long_output(self):
        testdir = os.path.join(self.common_test_dir, '254 long output')
        self.init(testdir)
        self.build()
        self.run_tests()

        # Ensure lines are found from testlog.txt when not being verbose.

        i = 1
        with open(os.path.join(self.logdir, 'testlog.txt'), encoding='utf-8') as f:
            line = f.readline()
            while line and i < 100001:
                if f'# Iteration {i} to stdout' in line:
                    i += 1
                line = f.readline()
            self.assertEqual(i, 100001)

            i = 1
            while line:
                if f'# Iteration {i} to stderr' in line:
                    i += 1
                line = f.readline()
        self.assertEqual(i, 100001)

        # Ensure lines are found from both testlog.txt and console when being verbose.

        out = self._run(self.mtest_command + ['-v'])
        i = 1
        with open(os.path.join(self.logdir, 'testlog.txt'), encoding='utf-8') as f:
            line = f.readline()
            while line and i < 100001:
                if f'# Iteration {i} to stdout' in line:
                    i += 1
                line = f.readline()
            self.assertEqual(i, 100001)

            i = 1
            while line:
                if f'# Iteration {i} to stderr' in line:
                    i += 1
                line = f.readline()
        self.assertEqual(i, 100001)

        lines = out.split('\n')
        line_number = 0
        i = 1
        while line_number < len(lines) and i < 100001:
            print('---> %s' % lines[line_number])
            if f'# Iteration {i} to stdout' in lines[line_number]:
                i += 1
            line_number += 1
        self.assertEqual(i, 100001)

        line_number = 0
        i = 1
        while line_number < len(lines):
            if f'# Iteration {i} to stderr' in lines[line_number]:
                i += 1
            line_number += 1
        self.assertEqual(i, 100001)


    def test_testsetups(self):
        if not shutil.which('valgrind'):
            raise SkipTest('Valgrind not installed.')
        testdir = os.path.join(self.unit_test_dir, '2 testsetups')
        self.init(testdir)
        self.build()
        # Run tests without setup
        self.run_tests()
        with open(os.path.join(self.logdir, 'testlog.txt'), encoding='utf-8') as f:
            basic_log = f.read()
        # Run buggy test with setup that has env that will make it fail
        self.assertRaises(subprocess.CalledProcessError,
                          self._run, self.mtest_command + ['--setup=valgrind'])
        with open(os.path.join(self.logdir, 'testlog-valgrind.txt'), encoding='utf-8') as f:
            vg_log = f.read()
        self.assertNotIn('TEST_ENV is set', basic_log)
        self.assertNotIn('Memcheck', basic_log)
        self.assertIn('TEST_ENV is set', vg_log)
        self.assertIn('Memcheck', vg_log)
        # Run buggy test with setup without env that will pass
        self._run(self.mtest_command + ['--setup=wrapper'])
        # Setup with no properties works
        self._run(self.mtest_command + ['--setup=empty'])
        # Setup with only env works
        self._run(self.mtest_command + ['--setup=onlyenv'])
        self._run(self.mtest_command + ['--setup=onlyenv2'])
        self._run(self.mtest_command + ['--setup=onlyenv3'])
        # Setup with only a timeout works
        self._run(self.mtest_command + ['--setup=timeout'])
        # Setup that does not define a wrapper works with --wrapper
        self._run(self.mtest_command + ['--setup=timeout', '--wrapper', shutil.which('valgrind')])
        # Setup that skips test works
        self._run(self.mtest_command + ['--setup=good'])
        with open(os.path.join(self.logdir, 'testlog-good.txt'), encoding='utf-8') as f:
            exclude_suites_log = f.read()
        self.assertNotIn('buggy', exclude_suites_log)
        # --suite overrides add_test_setup(exclude_suites)
        self._run(self.mtest_command + ['--setup=good', '--suite', 'buggy'])
        with open(os.path.join(self.logdir, 'testlog-good.txt'), encoding='utf-8') as f:
            include_suites_log = f.read()
        self.assertIn('buggy', include_suites_log)

    def test_testsetup_selection(self):
        testdir = os.path.join(self.unit_test_dir, '14 testsetup selection')
        self.init(testdir)
        self.build()

        # Run tests without setup
        self.run_tests()

        self.assertRaises(subprocess.CalledProcessError, self._run, self.mtest_command + ['--setup=missingfromfoo'])
        self._run(self.mtest_command + ['--setup=missingfromfoo', '--no-suite=foo:'])

        self._run(self.mtest_command + ['--setup=worksforall'])
        self._run(self.mtest_command + ['--setup=main:worksforall'])

        self.assertRaises(subprocess.CalledProcessError, self._run,
                          self.mtest_command + ['--setup=onlyinbar'])
        self.assertRaises(subprocess.CalledProcessError, self._run,
                          self.mtest_command + ['--setup=onlyinbar', '--no-suite=main:'])
        self._run(self.mtest_command + ['--setup=onlyinbar', '--no-suite=main:', '--no-suite=foo:'])
        self._run(self.mtest_command + ['--setup=bar:onlyinbar'])
        self.assertRaises(subprocess.CalledProcessError, self._run,
                          self.mtest_command + ['--setup=foo:onlyinbar'])
        self.assertRaises(subprocess.CalledProcessError, self._run,
                          self.mtest_command + ['--setup=main:onlyinbar'])

    def test_testsetup_default(self):
        testdir = os.path.join(self.unit_test_dir, '48 testsetup default')
        self.init(testdir)
        self.build()

        # Run tests without --setup will cause the default setup to be used
        self.run_tests()
        with open(os.path.join(self.logdir, 'testlog.txt'), encoding='utf-8') as f:
            default_log = f.read()

        # Run tests with explicitly using the same setup that is set as default
        self._run(self.mtest_command + ['--setup=mydefault'])
        with open(os.path.join(self.logdir, 'testlog-mydefault.txt'), encoding='utf-8') as f:
            mydefault_log = f.read()

        # Run tests with another setup
        self._run(self.mtest_command + ['--setup=other'])
        with open(os.path.join(self.logdir, 'testlog-other.txt'), encoding='utf-8') as f:
            other_log = f.read()

        self.assertIn('ENV_A is 1', default_log)
        self.assertIn('ENV_B is 2', default_log)
        self.assertIn('ENV_C is 2', default_log)

        self.assertIn('ENV_A is 1', mydefault_log)
        self.assertIn('ENV_B is 2', mydefault_log)
        self.assertIn('ENV_C is 2', mydefault_log)

        self.assertIn('ENV_A is 1', other_log)
        self.assertIn('ENV_B is 3', other_log)
        self.assertIn('ENV_C is 2', other_log)

    def assertFailedTestCount(self, failure_count, command):
        try:
            self._run(command)
            self.assertEqual(0, failure_count, 'Expected %d tests to fail.' % failure_count)
        except subprocess.CalledProcessError as e:
            self.assertEqual(e.returncode, failure_count)

    def test_suite_selection(self):
        testdir = os.path.join(self.unit_test_dir, '4 suite selection')
        self.init(testdir)
        self.build()

        self.assertFailedTestCount(4, self.mtest_command)

        self.assertFailedTestCount(0, self.mtest_command + ['--suite', ':success'])
        self.assertFailedTestCount(3, self.mtest_command + ['--suite', ':fail'])
        self.assertFailedTestCount(4, self.mtest_command + ['--no-suite', ':success'])
        self.assertFailedTestCount(1, self.mtest_command + ['--no-suite', ':fail'])

        self.assertFailedTestCount(1, self.mtest_command + ['--suite', 'mainprj'])
        self.assertFailedTestCount(0, self.mtest_command + ['--suite', 'subprjsucc'])
        self.assertFailedTestCount(1, self.mtest_command + ['--suite', 'subprjfail'])
        self.assertFailedTestCount(1, self.mtest_command + ['--suite', 'subprjmix'])
        self.assertFailedTestCount(3, self.mtest_command + ['--no-suite', 'mainprj'])
        self.assertFailedTestCount(4, self.mtest_command + ['--no-suite', 'subprjsucc'])
        self.assertFailedTestCount(3, self.mtest_command + ['--no-suite', 'subprjfail'])
        self.assertFailedTestCount(3, self.mtest_command + ['--no-suite', 'subprjmix'])

        self.assertFailedTestCount(1, self.mtest_command + ['--suite', 'mainprj:fail'])
        self.assertFailedTestCount(0, self.mtest_command + ['--suite', 'mainprj:success'])
        self.assertFailedTestCount(3, self.mtest_command + ['--no-suite', 'mainprj:fail'])
        self.assertFailedTestCount(4, self.mtest_command + ['--no-suite', 'mainprj:success'])

        self.assertFailedTestCount(1, self.mtest_command + ['--suite', 'subprjfail:fail'])
        self.assertFailedTestCount(0, self.mtest_command + ['--suite', 'subprjfail:success'])
        self.assertFailedTestCount(3, self.mtest_command + ['--no-suite', 'subprjfail:fail'])
        self.assertFailedTestCount(4, self.mtest_command + ['--no-suite', 'subprjfail:success'])

        self.assertFailedTestCount(0, self.mtest_command + ['--suite', 'subprjsucc:fail'])
        self.assertFailedTestCount(0, self.mtest_command + ['--suite', 'subprjsucc:success'])
        self.assertFailedTestCount(4, self.mtest_command + ['--no-suite', 'subprjsucc:fail'])
        self.assertFailedTestCount(4, self.mtest_command + ['--no-suite', 'subprjsucc:success'])

        self.assertFailedTestCount(1, self.mtest_command + ['--suite', 'subprjmix:fail'])
        self.assertFailedTestCount(0, self.mtest_command + ['--suite', 'subprjmix:success'])
        self.assertFailedTestCount(3, self.mtest_command + ['--no-suite', 'subprjmix:fail'])
        self.assertFailedTestCount(4, self.mtest_command + ['--no-suite', 'subprjmix:success'])

        self.assertFailedTestCount(2, self.mtest_command + ['--suite', 'subprjfail', '--suite', 'subprjmix:fail'])
        self.assertFailedTestCount(3, self.mtest_command + ['--suite', 'subprjfail', '--suite', 'subprjmix', '--suite', 'mainprj'])
        self.assertFailedTestCount(2, self.mtest_command + ['--suite', 'subprjfail', '--suite', 'subprjmix', '--suite', 'mainprj', '--no-suite', 'subprjmix:fail'])
        self.assertFailedTestCount(1, self.mtest_command + ['--suite', 'subprjfail', '--suite', 'subprjmix', '--suite', 'mainprj', '--no-suite', 'subprjmix:fail', 'mainprj-failing_test'])

        self.assertFailedTestCount(2, self.mtest_command + ['--no-suite', 'subprjfail:fail', '--no-suite', 'subprjmix:fail'])

    def test_mtest_reconfigure(self):
        if self.backend is not Backend.ninja:
            raise SkipTest(f'mtest can\'t rebuild with {self.backend.name!r}')

        testdir = os.path.join(self.common_test_dir, '206 tap tests')
        self.init(testdir)
        self.utime(os.path.join(testdir, 'meson.build'))
        o = self._run(self.mtest_command + ['--list'])
        self.assertIn('Regenerating build files.', o)
        self.assertIn('test_features / xfail', o)
        o = self._run(self.mtest_command + ['--list'])
        self.assertNotIn('Regenerating build files.', o)
        # no real targets should have been built
        tester = os.path.join(self.builddir, 'tester' + exe_suffix)
        self.assertPathDoesNotExist(tester)
        # check that we don't reconfigure if --no-rebuild is passed
        self.utime(os.path.join(testdir, 'meson.build'))
        o = self._run(self.mtest_command + ['--list', '--no-rebuild'])
        self.assertNotIn('Regenerating build files.', o)

    def test_unexisting_test_name(self):
        testdir = os.path.join(self.unit_test_dir, '4 suite selection')
        self.init(testdir)
        self.build()

        self.assertRaises(subprocess.CalledProcessError, self._run, self.mtest_command + ['notatest'])

    def test_select_test_using_wildcards(self):
        testdir = os.path.join(self.unit_test_dir, '4 suite selection')
        self.init(testdir)
        self.build()

        o = self._run(self.mtest_command + ['--list', 'mainprj*'])
        self.assertIn('mainprj-failing_test', o)
        self.assertIn('mainprj-successful_test_no_suite', o)
        self.assertNotIn('subprj', o)

        o = self._run(self.mtest_command + ['--list', '*succ*', 'subprjm*:'])
        self.assertIn('mainprj-successful_test_no_suite', o)
        self.assertIn('subprjmix-failing_test', o)
        self.assertIn('subprjmix-successful_test', o)
        self.assertIn('subprjsucc-successful_test_no_suite', o)
        self.assertNotIn('subprjfail-failing_test', o)

    def test_build_by_default(self):
        testdir = os.path.join(self.common_test_dir, '129 build by default')
        self.init(testdir)
        self.build()
        genfile1 = os.path.join(self.builddir, 'generated1.dat')
        genfile2 = os.path.join(self.builddir, 'generated2.dat')
        exe1 = os.path.join(self.builddir, 'fooprog' + exe_suffix)
        exe2 = os.path.join(self.builddir, 'barprog' + exe_suffix)
        self.assertPathExists(genfile1)
        self.assertPathExists(genfile2)
        self.assertPathDoesNotExist(exe1)
        self.assertPathDoesNotExist(exe2)
        self.build(target=('fooprog' + exe_suffix))
        self.assertPathExists(exe1)
        self.build(target=('barprog' + exe_suffix))
        self.assertPathExists(exe2)

    def test_build_generated_pyx_directly(self):
        # Check that the transpile stage also includes
        # dependencies for the compilation stage as dependencies
        testdir = os.path.join("test cases/cython", '2 generated sources')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        try:
            detect_compiler_for(env, "cython", MachineChoice.HOST, True, '')
        except EnvironmentException:
            raise SkipTest("Cython is not installed")
        self.init(testdir)
        # Need to get the full target name of the pyx.c target
        # (which is unfortunately not provided by introspection :( )
        # We'll need to dig into the generated sources
        targets = self.introspect('--targets')
        name = None
        for target in targets:
            for target_sources in target["target_sources"]:
                for generated_source in target_sources.get("generated_sources", []):
                    if "includestuff.pyx.c" in generated_source:
                        name = generated_source
                        break
        # Split the path (we only want the includestuff.cpython-blahblah.so.p/includestuff.pyx.c)
        name = os.path.normpath(name).split(os.sep)[-2:]
        name = os.sep.join(name)  # Glue list into a string
        self.build(target=name)

    def test_build_pyx_depfiles(self):
        # building regularly and then touching a depfile dependency should rebuild
        testdir = os.path.join("test cases/cython", '2 generated sources')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        try:
            cython = detect_compiler_for(env, "cython", MachineChoice.HOST, True, '')
            if not version_compare(cython.version, '>=0.29.33'):
                raise SkipTest('Cython is too old')
        except EnvironmentException:
            raise SkipTest("Cython is not installed")
        self.init(testdir)

        targets = self.introspect('--targets')
        for target in targets:
            if target['name'].startswith('simpleinclude'):
                name = target['name']
        self.build()
        self.utime(os.path.join(testdir, 'simplestuff.pxi'))
        self.assertBuildRelinkedOnlyTarget(name)


    def test_internal_include_order(self):
        if mesonbuild.environment.detect_msys2_arch() and ('MESON_RSP_THRESHOLD' in os.environ):
            raise SkipTest('Test does not yet support gcc rsp files on msys2')

        testdir = os.path.join(self.common_test_dir, '130 include order')
        self.init(testdir)
        execmd = fxecmd = None
        for cmd in self.get_compdb():
            if 'someexe' in cmd['command']:
                execmd = cmd['command']
                continue
            if 'somefxe' in cmd['command']:
                fxecmd = cmd['command']
                continue
        if not execmd or not fxecmd:
            raise Exception('Could not find someexe and somfxe commands')
        # Check include order for 'someexe'
        incs = [a for a in split_args(execmd) if a.startswith("-I")]
        self.assertEqual(len(incs), 9)
        # Need to run the build so the private dir is created.
        self.build()
        pdirs = glob(os.path.join(self.builddir, 'sub4/someexe*.p'))
        self.assertEqual(len(pdirs), 1)
        privdir = pdirs[0][len(self.builddir)+1:]
        self.assertPathEqual(incs[0], "-I" + privdir)
        # target build subdir
        self.assertPathEqual(incs[1], "-Isub4")
        # target source subdir
        self.assertPathBasenameEqual(incs[2], 'sub4')
        # include paths added via per-target c_args: ['-I'...]
        self.assertPathBasenameEqual(incs[3], 'sub3')
        # target include_directories: build dir
        self.assertPathEqual(incs[4], "-Isub2")
        # target include_directories: source dir
        self.assertPathBasenameEqual(incs[5], 'sub2')
        # target internal dependency include_directories: build dir
        self.assertPathEqual(incs[6], "-Isub1")
        # target internal dependency include_directories: source dir
        self.assertPathBasenameEqual(incs[7], 'sub1')
        # custom target include dir
        self.assertPathEqual(incs[8], '-Ictsub')
        # Check include order for 'somefxe'
        incs = [a for a in split_args(fxecmd) if a.startswith('-I')]
        self.assertEqual(len(incs), 9)
        # target private dir
        pdirs = glob(os.path.join(self.builddir, 'somefxe*.p'))
        self.assertEqual(len(pdirs), 1)
        privdir = pdirs[0][len(self.builddir)+1:]
        self.assertPathEqual(incs[0], '-I' + privdir)
        # target build dir
        self.assertPathEqual(incs[1], '-I.')
        # target source dir
        self.assertPathBasenameEqual(incs[2], os.path.basename(testdir))
        # target internal dependency correct include_directories: build dir
        self.assertPathEqual(incs[3], "-Isub4")
        # target internal dependency correct include_directories: source dir
        self.assertPathBasenameEqual(incs[4], 'sub4')
        # target internal dependency dep include_directories: build dir
        self.assertPathEqual(incs[5], "-Isub1")
        # target internal dependency dep include_directories: source dir
        self.assertPathBasenameEqual(incs[6], 'sub1')
        # target internal dependency wrong include_directories: build dir
        self.assertPathEqual(incs[7], "-Isub2")
        # target internal dependency wrong include_directories: source dir
        self.assertPathBasenameEqual(incs[8], 'sub2')

    def test_compiler_detection(self):
        '''
        Test that automatic compiler detection and setting from the environment
        both work just fine. This is needed because while running project tests
        and other unit tests, we always read CC/CXX/etc from the environment.
        '''
        gnu = GnuCompiler
        clang = ClangCompiler
        intel = IntelGnuLikeCompiler
        msvc = (VisualStudioCCompiler, VisualStudioCPPCompiler)
        clangcl = (ClangClCCompiler, ClangClCPPCompiler)
        ar = linkers.ArLinker
        lib = linkers.VisualStudioLinker
        langs = [('c', 'CC'), ('cpp', 'CXX')]
        if not is_windows() and platform.machine().lower() != 'e2k':
            langs += [('objc', 'OBJC'), ('objcpp', 'OBJCXX')]
        testdir = os.path.join(self.unit_test_dir, '5 compiler detection')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        for lang, evar in langs:
            # Detect with evar and do sanity checks on that
            if evar in os.environ:
                ecc = compiler_from_language(env, lang, MachineChoice.HOST)
                self.assertTrue(ecc.version)
                elinker = detect_static_linker(env, ecc)
                # Pop it so we don't use it for the next detection
                evalue = os.environ.pop(evar)
                # Very rough/strict heuristics. Would never work for actual
                # compiler detection, but should be ok for the tests.
                ebase = os.path.basename(evalue)
                if ebase.startswith('g') or ebase.endswith(('-gcc', '-g++')):
                    self.assertIsInstance(ecc, gnu)
                    self.assertIsInstance(elinker, ar)
                elif 'clang-cl' in ebase:
                    self.assertIsInstance(ecc, clangcl)
                    self.assertIsInstance(elinker, lib)
                elif 'clang' in ebase:
                    self.assertIsInstance(ecc, clang)
                    self.assertIsInstance(elinker, ar)
                elif ebase.startswith('ic'):
                    self.assertIsInstance(ecc, intel)
                    self.assertIsInstance(elinker, ar)
                elif ebase.startswith('cl'):
                    self.assertIsInstance(ecc, msvc)
                    self.assertIsInstance(elinker, lib)
                else:
                    raise AssertionError(f'Unknown compiler {evalue!r}')
                # Check that we actually used the evalue correctly as the compiler
                self.assertEqual(ecc.get_exelist(), split_args(evalue))
            # Do auto-detection of compiler based on platform, PATH, etc.
            cc = compiler_from_language(env, lang, MachineChoice.HOST)
            self.assertTrue(cc.version)
            linker = detect_static_linker(env, cc)
            # Check compiler type
            if isinstance(cc, gnu):
                self.assertIsInstance(linker, ar)
                if is_osx():
                    self.assertIsInstance(cc.linker, linkers.AppleDynamicLinker)
                elif is_sunos():
                    self.assertIsInstance(cc.linker, (linkers.SolarisDynamicLinker, linkers.GnuLikeDynamicLinkerMixin))
                else:
                    self.assertIsInstance(cc.linker, linkers.GnuLikeDynamicLinkerMixin)
            if isinstance(cc, clangcl):
                self.assertIsInstance(linker, lib)
                self.assertIsInstance(cc.linker, linkers.ClangClDynamicLinker)
            if isinstance(cc, clang):
                self.assertIsInstance(linker, ar)
                if is_osx():
                    self.assertIsInstance(cc.linker, linkers.AppleDynamicLinker)
                elif is_windows():
                    # This is clang, not clang-cl. This can be either an
                    # ld-like linker of link.exe-like linker (usually the
                    # former for msys2, the latter otherwise)
                    self.assertIsInstance(cc.linker, (linkers.MSVCDynamicLinker, linkers.GnuLikeDynamicLinkerMixin))
                else:
                    self.assertIsInstance(cc.linker, linkers.GnuLikeDynamicLinkerMixin)
            if isinstance(cc, intel):
                self.assertIsInstance(linker, ar)
                if is_osx():
                    self.assertIsInstance(cc.linker, linkers.AppleDynamicLinker)
                elif is_windows():
                    self.assertIsInstance(cc.linker, linkers.XilinkDynamicLinker)
                else:
                    self.assertIsInstance(cc.linker, linkers.GnuDynamicLinker)
            if isinstance(cc, msvc):
                self.assertTrue(is_windows())
                self.assertIsInstance(linker, lib)
                self.assertEqual(cc.id, 'msvc')
                self.assertTrue(hasattr(cc, 'is_64'))
                self.assertIsInstance(cc.linker, linkers.MSVCDynamicLinker)
                # If we're on Windows CI, we know what the compiler will be
                if 'arch' in os.environ:
                    if os.environ['arch'] == 'x64':
                        self.assertTrue(cc.is_64)
                    else:
                        self.assertFalse(cc.is_64)
            # Set evar ourselves to a wrapper script that just calls the same
            # exelist + some argument. This is meant to test that setting
            # something like `ccache gcc -pipe` or `distcc ccache gcc` works.
            wrapper = os.path.join(testdir, 'compiler wrapper.py')
            wrappercc = python_command + [wrapper] + cc.get_exelist() + ['-DSOME_ARG']
            os.environ[evar] = ' '.join(quote_arg(w) for w in wrappercc)

            # Check static linker too
            wrapperlinker = python_command + [wrapper] + linker.get_exelist() + linker.get_always_args()
            os.environ['AR'] = ' '.join(quote_arg(w) for w in wrapperlinker)

            # Need a new env to re-run environment loading
            env = get_fake_env(testdir, self.builddir, self.prefix)

            wcc = compiler_from_language(env, lang, MachineChoice.HOST)
            wlinker = detect_static_linker(env, wcc)
            # Pop it so we don't use it for the next detection
            os.environ.pop('AR')
            # Must be the same type since it's a wrapper around the same exelist
            self.assertIs(type(cc), type(wcc))
            self.assertIs(type(linker), type(wlinker))
            # Ensure that the exelist is correct
            self.assertEqual(wcc.get_exelist(), wrappercc)
            self.assertEqual(wlinker.get_exelist(), wrapperlinker)
            # Ensure that the version detection worked correctly
            self.assertEqual(cc.version, wcc.version)
            if hasattr(cc, 'is_64'):
                self.assertEqual(cc.is_64, wcc.is_64)

    def test_always_prefer_c_compiler_for_asm(self):
        testdir = os.path.join(self.common_test_dir, '133 c cpp and asm')
        # Skip if building with MSVC
        env = get_fake_env(testdir, self.builddir, self.prefix)
        if detect_c_compiler(env, MachineChoice.HOST).get_id() == 'msvc':
            raise SkipTest('MSVC can\'t compile assembly')
        self.init(testdir)
        commands = {'c-asm': {}, 'cpp-asm': {}, 'cpp-c-asm': {}, 'c-cpp-asm': {}}
        for cmd in self.get_compdb():
            # Get compiler
            split = split_args(cmd['command'])
            if split[0] in ('ccache', 'sccache'):
                compiler = split[1]
            else:
                compiler = split[0]
            # Classify commands
            if 'Ic-asm' in cmd['command']:
                if cmd['file'].endswith('.S'):
                    commands['c-asm']['asm'] = compiler
                elif cmd['file'].endswith('.c'):
                    commands['c-asm']['c'] = compiler
                else:
                    raise AssertionError('{!r} found in cpp-asm?'.format(cmd['command']))
            elif 'Icpp-asm' in cmd['command']:
                if cmd['file'].endswith('.S'):
                    commands['cpp-asm']['asm'] = compiler
                elif cmd['file'].endswith('.cpp'):
                    commands['cpp-asm']['cpp'] = compiler
                else:
                    raise AssertionError('{!r} found in cpp-asm?'.format(cmd['command']))
            elif 'Ic-cpp-asm' in cmd['command']:
                if cmd['file'].endswith('.S'):
                    commands['c-cpp-asm']['asm'] = compiler
                elif cmd['file'].endswith('.c'):
                    commands['c-cpp-asm']['c'] = compiler
                elif cmd['file'].endswith('.cpp'):
                    commands['c-cpp-asm']['cpp'] = compiler
                else:
                    raise AssertionError('{!r} found in c-cpp-asm?'.format(cmd['command']))
            elif 'Icpp-c-asm' in cmd['command']:
                if cmd['file'].endswith('.S'):
                    commands['cpp-c-asm']['asm'] = compiler
                elif cmd['file'].endswith('.c'):
                    commands['cpp-c-asm']['c'] = compiler
                elif cmd['file'].endswith('.cpp'):
                    commands['cpp-c-asm']['cpp'] = compiler
                else:
                    raise AssertionError('{!r} found in cpp-c-asm?'.format(cmd['command']))
            else:
                raise AssertionError('Unknown command {!r} found'.format(cmd['command']))
        # Check that .S files are always built with the C compiler
        self.assertEqual(commands['c-asm']['asm'], commands['c-asm']['c'])
        self.assertEqual(commands['c-asm']['asm'], commands['cpp-asm']['asm'])
        self.assertEqual(commands['cpp-asm']['asm'], commands['c-cpp-asm']['c'])
        self.assertEqual(commands['c-cpp-asm']['asm'], commands['c-cpp-asm']['c'])
        self.assertEqual(commands['cpp-c-asm']['asm'], commands['cpp-c-asm']['c'])
        self.assertNotEqual(commands['cpp-asm']['asm'], commands['cpp-asm']['cpp'])
        self.assertNotEqual(commands['c-cpp-asm']['c'], commands['c-cpp-asm']['cpp'])
        self.assertNotEqual(commands['cpp-c-asm']['c'], commands['cpp-c-asm']['cpp'])
        # Check that the c-asm target is always linked with the C linker
        build_ninja = os.path.join(self.builddir, 'build.ninja')
        with open(build_ninja, encoding='utf-8') as f:
            contents = f.read()
            m = re.search('build c-asm.*: c_LINKER', contents)
        self.assertIsNotNone(m, msg=contents)

    def test_preprocessor_checks_CPPFLAGS(self):
        '''
        Test that preprocessor compiler checks read CPPFLAGS and also CFLAGS/CXXFLAGS but
        not LDFLAGS.
        '''
        testdir = os.path.join(self.common_test_dir, '132 get define')
        define = 'MESON_TEST_DEFINE_VALUE'
        # NOTE: this list can't have \n, ' or "
        # \n is never substituted by the GNU pre-processor via a -D define
        # ' and " confuse split_args() even when they are escaped
        # % and # confuse the MSVC preprocessor
        # !, ^, *, and < confuse lcc preprocessor
        value = 'spaces and fun@$&()-=_+{}[]:;>?,./~`'
        for env_var in [{'CPPFLAGS'}, {'CFLAGS', 'CXXFLAGS'}]:
            env = {}
            for i in env_var:
                env[i] = f'-D{define}="{value}"'
            env['LDFLAGS'] = '-DMESON_FAIL_VALUE=cflags-read'
            self.init(testdir, extra_args=[f'-D{define}={value}'], override_envvars=env)
            self.new_builddir()

    def test_custom_target_exe_data_deterministic(self):
        testdir = os.path.join(self.common_test_dir, '109 custom target capture')
        self.init(testdir)
        meson_exe_dat1 = glob(os.path.join(self.privatedir, 'meson_exe*.dat'))
        self.wipe()
        self.init(testdir)
        meson_exe_dat2 = glob(os.path.join(self.privatedir, 'meson_exe*.dat'))
        self.assertListEqual(meson_exe_dat1, meson_exe_dat2)

    def test_noop_changes_cause_no_rebuilds(self):
        '''
        Test that no-op changes to the build files such as mtime do not cause
        a rebuild of anything.
        '''
        testdir = os.path.join(self.common_test_dir, '6 linkshared')
        self.init(testdir)
        self.build()
        # Immediately rebuilding should not do anything
        self.assertBuildIsNoop()
        # Changing mtime of meson.build should not rebuild anything
        self.utime(os.path.join(testdir, 'meson.build'))
        self.assertReconfiguredBuildIsNoop()
        # Changing mtime of libefile.c should rebuild the library, but not relink the executable
        self.utime(os.path.join(testdir, 'libfile.c'))
        self.assertBuildRelinkedOnlyTarget('mylib')

    def test_source_changes_cause_rebuild(self):
        '''
        Test that changes to sources and headers cause rebuilds, but not
        changes to unused files (as determined by the dependency file) in the
        input files list.
        '''
        testdir = os.path.join(self.common_test_dir, '19 header in file list')
        self.init(testdir)
        self.build()
        # Immediately rebuilding should not do anything
        self.assertBuildIsNoop()
        # Changing mtime of header.h should rebuild everything
        self.utime(os.path.join(testdir, 'header.h'))
        self.assertBuildRelinkedOnlyTarget('prog')

    def test_custom_target_changes_cause_rebuild(self):
        '''
        Test that in a custom target, changes to the input files, the
        ExternalProgram, and any File objects on the command-line cause
        a rebuild.
        '''
        testdir = os.path.join(self.common_test_dir, '57 custom header generator')
        self.init(testdir)
        self.build()
        # Immediately rebuilding should not do anything
        self.assertBuildIsNoop()
        # Changing mtime of these should rebuild everything
        for f in ('input.def', 'makeheader.py', 'somefile.txt'):
            self.utime(os.path.join(testdir, f))
            self.assertBuildRelinkedOnlyTarget('prog')

    def test_source_generator_program_cause_rebuild(self):
        '''
        Test that changes to generator programs in the source tree cause
        a rebuild.
        '''
        testdir = os.path.join(self.common_test_dir, '90 gen extra')
        self.init(testdir)
        self.build()
        # Immediately rebuilding should not do anything
        self.assertBuildIsNoop()
        # Changing mtime of generator should rebuild the executable
        self.utime(os.path.join(testdir, 'srcgen.py'))
        self.assertRebuiltTarget('basic')

    def test_static_library_lto(self):
        '''
        Test that static libraries can be built with LTO and linked to
        executables. On Linux, this requires the use of gcc-ar.
        https://github.com/mesonbuild/meson/issues/1646
        '''
        testdir = os.path.join(self.common_test_dir, '5 linkstatic')

        env = get_fake_env(testdir, self.builddir, self.prefix)
        if detect_c_compiler(env, MachineChoice.HOST).get_id() == 'clang' and is_windows():
            raise SkipTest('LTO not (yet) supported by windows clang')

        self.init(testdir, extra_args='-Db_lto=true')
        self.build()
        self.run_tests()

    @skip_if_not_base_option('b_lto_threads')
    def test_lto_threads(self):
        testdir = os.path.join(self.common_test_dir, '6 linkshared')

        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        extra_args: T.List[str] = []
        if cc.get_id() == 'clang':
            if is_windows():
                raise SkipTest('LTO not (yet) supported by windows clang')

        self.init(testdir, extra_args=['-Db_lto=true', '-Db_lto_threads=8'] + extra_args)
        self.build()
        self.run_tests()

        expected = set(cc.get_lto_compile_args(threads=8))
        targets = self.introspect('--targets')
        # This assumes all of the targets support lto
        for t in targets:
            for s in t['target_sources']:
                if 'linker' in s:
                    continue
                for e in expected:
                    self.assertIn(e, s['parameters'])

    @skip_if_not_base_option('b_lto_mode')
    @skip_if_not_base_option('b_lto_threads')
    def test_lto_mode(self):
        testdir = os.path.join(self.common_test_dir, '6 linkshared')

        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        if cc.get_id() != 'clang':
            raise SkipTest('Only clang currently supports thinLTO')
        if cc.linker.id not in {'ld.lld', 'ld.gold', 'ld64', 'lld-link'}:
            raise SkipTest('thinLTO requires ld.lld, ld.gold, ld64, or lld-link')
        elif is_windows():
            raise SkipTest('LTO not (yet) supported by windows clang')

        self.init(testdir, extra_args=['-Db_lto=true', '-Db_lto_mode=thin', '-Db_lto_threads=8', '-Dc_args=-Werror=unused-command-line-argument'])
        self.build()
        self.run_tests()

        expected = set(cc.get_lto_compile_args(threads=8, mode='thin'))
        targets = self.introspect('--targets')
        # This assumes all of the targets support lto
        for t in targets:
            for src in t['target_sources']:
                self.assertTrue(expected.issubset(set(src['parameters'])), f'Incorrect values for {t["name"]}')

    def test_dist_git(self):
        if not shutil.which('git'):
            raise SkipTest('Git not found')
        if self.backend is not Backend.ninja:
            raise SkipTest('Dist is only supported with Ninja')

        try:
            self.dist_impl(git_init, _git_add_all)
        except PermissionError:
            # When run under Windows CI, something (virus scanner?)
            # holds on to the git files so cleaning up the dir
            # fails sometimes.
            pass

    def has_working_hg(self):
        if not shutil.which('hg'):
            return False
        try:
            # This check should not be necessary, but
            # CI under macOS passes the above test even
            # though Mercurial is not installed.
            if subprocess.call(['hg', '--version'],
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL) != 0:
                return False
            return True
        except FileNotFoundError:
            return False

    def test_dist_hg(self):
        if not self.has_working_hg():
            raise SkipTest('Mercurial not found or broken.')
        if self.backend is not Backend.ninja:
            raise SkipTest('Dist is only supported with Ninja')

        def hg_init(project_dir):
            subprocess.check_call(['hg', 'init'], cwd=project_dir)
            with open(os.path.join(project_dir, '.hg', 'hgrc'), 'w', encoding='utf-8') as f:
                print('[ui]', file=f)
                print('username=Author Person <teh_coderz@example.com>', file=f)
            subprocess.check_call(['hg', 'add', 'meson.build', 'distexe.c'], cwd=project_dir)
            subprocess.check_call(['hg', 'commit', '-m', 'I am a project'], cwd=project_dir)

        try:
            self.dist_impl(hg_init, include_subprojects=False)
        except PermissionError:
            # When run under Windows CI, something (virus scanner?)
            # holds on to the hg files so cleaning up the dir
            # fails sometimes.
            pass

    def test_dist_git_script(self):
        if not shutil.which('git'):
            raise SkipTest('Git not found')
        if self.backend is not Backend.ninja:
            raise SkipTest('Dist is only supported with Ninja')

        try:
            with tempfile.TemporaryDirectory() as tmpdir:
                project_dir = os.path.join(tmpdir, 'a')
                shutil.copytree(os.path.join(self.unit_test_dir, '35 dist script'),
                                project_dir)
                git_init(project_dir)
                self.init(project_dir)
                self.build('dist')

                self.new_builddir()
                self.init(project_dir, extra_args=['-Dsub:broken_dist_script=false'])
                self._run(self.meson_command + ['dist', '--include-subprojects'], workdir=self.builddir)
        except PermissionError:
            # When run under Windows CI, something (virus scanner?)
            # holds on to the git files so cleaning up the dir
            # fails sometimes.
            pass

    def create_dummy_subproject(self, project_dir, name):
        path = os.path.join(project_dir, 'subprojects', name)
        os.makedirs(path)
        with open(os.path.join(path, 'meson.build'), 'w', encoding='utf-8') as ofile:
            ofile.write(f"project('{name}', version: '1.0')")
        return path

    def dist_impl(self, vcs_init, vcs_add_all=None, include_subprojects=True):
        # Create this on the fly because having rogue .git directories inside
        # the source tree leads to all kinds of trouble.
        with tempfile.TemporaryDirectory() as project_dir:
            with open(os.path.join(project_dir, 'meson.build'), 'w', encoding='utf-8') as ofile:
                ofile.write(textwrap.dedent('''\
                    project('disttest', 'c', version : '1.4.3')
                    e = executable('distexe', 'distexe.c')
                    test('dist test', e)
                    subproject('vcssub', required : false)
                    subproject('tarballsub', required : false)
                    subproject('samerepo', required : false)
                    '''))
            with open(os.path.join(project_dir, 'distexe.c'), 'w', encoding='utf-8') as ofile:
                ofile.write(textwrap.dedent('''\
                    #include<stdio.h>

                    int main(int argc, char **argv) {
                        printf("I am a distribution test.\\n");
                        return 0;
                    }
                    '''))
            xz_distfile = os.path.join(self.distdir, 'disttest-1.4.3.tar.xz')
            xz_checksumfile = xz_distfile + '.sha256sum'
            gz_distfile = os.path.join(self.distdir, 'disttest-1.4.3.tar.gz')
            gz_checksumfile = gz_distfile + '.sha256sum'
            zip_distfile = os.path.join(self.distdir, 'disttest-1.4.3.zip')
            zip_checksumfile = zip_distfile + '.sha256sum'
            vcs_init(project_dir)
            if include_subprojects:
                vcs_init(self.create_dummy_subproject(project_dir, 'vcssub'))
                self.create_dummy_subproject(project_dir, 'tarballsub')
                self.create_dummy_subproject(project_dir, 'unusedsub')
            if vcs_add_all:
                vcs_add_all(self.create_dummy_subproject(project_dir, 'samerepo'))
            self.init(project_dir)
            self.build('dist')
            self.assertPathExists(xz_distfile)
            self.assertPathExists(xz_checksumfile)
            self.assertPathDoesNotExist(gz_distfile)
            self.assertPathDoesNotExist(gz_checksumfile)
            self.assertPathDoesNotExist(zip_distfile)
            self.assertPathDoesNotExist(zip_checksumfile)
            self._run(self.meson_command + ['dist', '--formats', 'gztar'],
                      workdir=self.builddir)
            self.assertPathExists(gz_distfile)
            self.assertPathExists(gz_checksumfile)
            self._run(self.meson_command + ['dist', '--formats', 'zip'],
                      workdir=self.builddir)
            self.assertPathExists(zip_distfile)
            self.assertPathExists(zip_checksumfile)
            os.remove(xz_distfile)
            os.remove(xz_checksumfile)
            os.remove(gz_distfile)
            os.remove(gz_checksumfile)
            os.remove(zip_distfile)
            os.remove(zip_checksumfile)
            self._run(self.meson_command + ['dist', '--formats', 'xztar,gztar,zip'],
                      workdir=self.builddir)
            self.assertPathExists(xz_distfile)
            self.assertPathExists(xz_checksumfile)
            self.assertPathExists(gz_distfile)
            self.assertPathExists(gz_checksumfile)
            self.assertPathExists(zip_distfile)
            self.assertPathExists(zip_checksumfile)

            if include_subprojects:
                # Verify that without --include-subprojects we have files from
                # the main project and also files from subprojects part of the
                # main vcs repository.
                z = zipfile.ZipFile(zip_distfile)
                expected = ['disttest-1.4.3/',
                            'disttest-1.4.3/meson.build',
                            'disttest-1.4.3/distexe.c']
                if vcs_add_all:
                    expected += ['disttest-1.4.3/subprojects/',
                                 'disttest-1.4.3/subprojects/samerepo/',
                                 'disttest-1.4.3/subprojects/samerepo/meson.build']
                self.assertEqual(sorted(expected),
                                 sorted(z.namelist()))
                # Verify that with --include-subprojects we now also have files
                # from tarball and separate vcs subprojects. But not files from
                # unused subprojects.
                self._run(self.meson_command + ['dist', '--formats', 'zip', '--include-subprojects'],
                          workdir=self.builddir)
                z = zipfile.ZipFile(zip_distfile)
                expected += ['disttest-1.4.3/subprojects/tarballsub/',
                             'disttest-1.4.3/subprojects/tarballsub/meson.build',
                             'disttest-1.4.3/subprojects/vcssub/',
                             'disttest-1.4.3/subprojects/vcssub/meson.build']
                self.assertEqual(sorted(expected),
                                 sorted(z.namelist()))
            if vcs_add_all:
                # Verify we can distribute separately subprojects in the same vcs
                # repository as the main project.
                subproject_dir = os.path.join(project_dir, 'subprojects', 'samerepo')
                self.new_builddir()
                self.init(subproject_dir)
                self.build('dist')
                xz_distfile = os.path.join(self.distdir, 'samerepo-1.0.tar.xz')
                xz_checksumfile = xz_distfile + '.sha256sum'
                self.assertPathExists(xz_distfile)
                self.assertPathExists(xz_checksumfile)
                tar = tarfile.open(xz_distfile, "r:xz")  # [ignore encoding]
                self.assertEqual(sorted(['samerepo-1.0',
                                         'samerepo-1.0/meson.build']),
                                 sorted(i.name for i in tar))

    def test_rpath_uses_ORIGIN(self):
        '''
        Test that built targets use $ORIGIN in rpath, which ensures that they
        are relocatable and ensures that builds are reproducible since the
        build directory won't get embedded into the built binaries.
        '''
        if is_windows() or is_cygwin():
            raise SkipTest('Windows PE/COFF binaries do not use RPATH')
        testdir = os.path.join(self.common_test_dir, '39 library chain')
        self.init(testdir)
        self.build()
        for each in ('prog', 'subdir/liblib1.so', ):
            rpath = get_rpath(os.path.join(self.builddir, each))
            self.assertTrue(rpath, f'Rpath could not be determined for {each}.')
            if is_dragonflybsd():
                # DragonflyBSD will prepend /usr/lib/gccVERSION to the rpath,
                # so ignore that.
                self.assertTrue(rpath.startswith('/usr/lib/gcc'))
                rpaths = rpath.split(':')[1:]
            else:
                rpaths = rpath.split(':')
            for path in rpaths:
                self.assertTrue(path.startswith('$ORIGIN'), msg=(each, path))
        # These two don't link to anything else, so they do not need an rpath entry.
        for each in ('subdir/subdir2/liblib2.so', 'subdir/subdir3/liblib3.so'):
            rpath = get_rpath(os.path.join(self.builddir, each))
            if is_dragonflybsd():
                # The rpath should be equal to /usr/lib/gccVERSION
                self.assertTrue(rpath.startswith('/usr/lib/gcc'))
                self.assertEqual(len(rpath.split(':')), 1)
            else:
                self.assertIsNone(rpath)

    def test_dash_d_dedup(self):
        testdir = os.path.join(self.unit_test_dir, '9 d dedup')
        self.init(testdir)
        cmd = self.get_compdb()[0]['command']
        self.assertTrue('-D FOO -D BAR' in cmd or
                        '"-D" "FOO" "-D" "BAR"' in cmd or
                        '/D FOO /D BAR' in cmd or
                        '"/D" "FOO" "/D" "BAR"' in cmd)

    def test_all_forbidden_targets_tested(self):
        '''
        Test that all forbidden targets are tested in the '150 reserved targets'
        test. Needs to be a unit test because it accesses Meson internals.
        '''
        testdir = os.path.join(self.common_test_dir, '150 reserved targets')
        targets = set(mesonbuild.coredata.FORBIDDEN_TARGET_NAMES)
        # We don't actually define a target with this name
        targets.remove('build.ninja')
        # Remove this to avoid multiple entries with the same name
        # but different case.
        targets.remove('PHONY')
        for i in targets:
            self.assertPathExists(os.path.join(testdir, i))

    def detect_prebuild_env(self):
        env = get_fake_env()
        cc = detect_c_compiler(env, MachineChoice.HOST)
        stlinker = detect_static_linker(env, cc)
        if is_windows():
            object_suffix = 'obj'
            shared_suffix = 'dll'
        elif is_cygwin():
            object_suffix = 'o'
            shared_suffix = 'dll'
        elif is_osx():
            object_suffix = 'o'
            shared_suffix = 'dylib'
        else:
            object_suffix = 'o'
            shared_suffix = 'so'
        return (cc, stlinker, object_suffix, shared_suffix)

    def detect_prebuild_env_versioned(self):
        (cc, stlinker, object_suffix, shared_suffix) = self.detect_prebuild_env()
        shared_suffixes = [shared_suffix]
        if shared_suffix == 'so':
            # .so may have version information integrated into the filename
            shared_suffixes += ['so.1', 'so.1.2.3', '1.so', '1.so.2.3']
        return (cc, stlinker, object_suffix, shared_suffixes)

    def pbcompile(self, compiler, source, objectfile, extra_args=None):
        cmd = compiler.get_exelist()
        extra_args = extra_args or []
        if compiler.get_argument_syntax() == 'msvc':
            cmd += ['/nologo', '/Fo' + objectfile, '/c', source] + extra_args
        else:
            cmd += ['-c', source, '-o', objectfile] + extra_args
        subprocess.check_call(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def test_prebuilt_object(self):
        (compiler, _, object_suffix, _) = self.detect_prebuild_env()
        tdir = os.path.join(self.unit_test_dir, '15 prebuilt object')
        source = os.path.join(tdir, 'source.c')
        objectfile = os.path.join(tdir, 'prebuilt.' + object_suffix)
        self.pbcompile(compiler, source, objectfile)
        try:
            self.init(tdir)
            self.build()
            self.run_tests()
        finally:
            os.unlink(objectfile)

    def build_static_lib(self, compiler, linker, source, objectfile, outfile, extra_args=None):
        if extra_args is None:
            extra_args = []
        link_cmd = linker.get_exelist()
        link_cmd += linker.get_always_args()
        link_cmd += linker.get_std_link_args(get_fake_env(), False)
        link_cmd += linker.get_output_args(outfile)
        link_cmd += [objectfile]
        self.pbcompile(compiler, source, objectfile, extra_args=extra_args)
        try:
            subprocess.check_call(link_cmd)
        finally:
            os.unlink(objectfile)

    def test_prebuilt_static_lib(self):
        (cc, stlinker, object_suffix, _) = self.detect_prebuild_env()
        tdir = os.path.join(self.unit_test_dir, '16 prebuilt static')
        source = os.path.join(tdir, 'libdir/best.c')
        objectfile = os.path.join(tdir, 'libdir/best.' + object_suffix)
        stlibfile = os.path.join(tdir, 'libdir/libbest.a')
        self.build_static_lib(cc, stlinker, source, objectfile, stlibfile)
        # Run the test
        try:
            self.init(tdir)
            self.build()
            self.run_tests()
        finally:
            os.unlink(stlibfile)

    def build_shared_lib(self, compiler, source, objectfile, outfile, impfile, extra_args=None):
        if extra_args is None:
            extra_args = []
        if compiler.get_argument_syntax() == 'msvc':
            link_cmd = compiler.get_linker_exelist() + [
                '/NOLOGO', '/DLL', '/DEBUG', '/IMPLIB:' + impfile,
                '/OUT:' + outfile, objectfile]
        else:
            if not (compiler.info.is_windows() or compiler.info.is_cygwin() or compiler.info.is_darwin()):
                extra_args += ['-fPIC']
            link_cmd = compiler.get_exelist() + ['-shared', '-o', outfile, objectfile]
            if not is_osx():
                link_cmd += ['-Wl,-soname=' + os.path.basename(outfile)]
        self.pbcompile(compiler, source, objectfile, extra_args=extra_args)
        try:
            subprocess.check_call(link_cmd)
        finally:
            os.unlink(objectfile)

    def test_prebuilt_shared_lib(self):
        (cc, _, object_suffix, shared_suffix) = self.detect_prebuild_env()
        tdir = os.path.join(self.unit_test_dir, '17 prebuilt shared')
        source = os.path.join(tdir, 'alexandria.c')
        objectfile = os.path.join(tdir, 'alexandria.' + object_suffix)
        impfile = os.path.join(tdir, 'alexandria.lib')
        if cc.get_argument_syntax() == 'msvc':
            shlibfile = os.path.join(tdir, 'alexandria.' + shared_suffix)
        elif is_cygwin():
            shlibfile = os.path.join(tdir, 'cygalexandria.' + shared_suffix)
        else:
            shlibfile = os.path.join(tdir, 'libalexandria.' + shared_suffix)
        self.build_shared_lib(cc, source, objectfile, shlibfile, impfile)

        if is_windows():
            def cleanup() -> None:
                """Clean up all the garbage MSVC writes in the source tree."""

                for fname in glob(os.path.join(tdir, 'alexandria.*')):
                    if os.path.splitext(fname)[1] not in {'.c', '.h'}:
                        os.unlink(fname)
            self.addCleanup(cleanup)
        else:
            self.addCleanup(os.unlink, shlibfile)

        # Run the test
        self.init(tdir)
        self.build()
        self.run_tests()

    def test_prebuilt_shared_lib_rpath(self) -> None:
        (cc, _, object_suffix, shared_suffix) = self.detect_prebuild_env()
        tdir = os.path.join(self.unit_test_dir, '17 prebuilt shared')
        with tempfile.TemporaryDirectory() as d:
            source = os.path.join(tdir, 'alexandria.c')
            objectfile = os.path.join(d, 'alexandria.' + object_suffix)
            impfile = os.path.join(d, 'alexandria.lib')
            if cc.get_argument_syntax() == 'msvc':
                shlibfile = os.path.join(d, 'alexandria.' + shared_suffix)
            elif is_cygwin():
                shlibfile = os.path.join(d, 'cygalexandria.' + shared_suffix)
            else:
                shlibfile = os.path.join(d, 'libalexandria.' + shared_suffix)
            # Ensure MSVC extra files end up in the directory that gets deleted
            # at the end
            with chdir(d):
                self.build_shared_lib(cc, source, objectfile, shlibfile, impfile)

            # Run the test
            self.init(tdir, extra_args=[f'-Dsearch_dir={d}'])
            self.build()
            self.run_tests()

    @skipIfNoPkgconfig
    def test_prebuilt_shared_lib_pkg_config(self) -> None:
        (cc, _, object_suffix, shared_suffixes) = self.detect_prebuild_env_versioned()
        tdir = os.path.join(self.unit_test_dir, '17 prebuilt shared')
        for shared_suffix in shared_suffixes:
            with tempfile.TemporaryDirectory() as d:
                source = os.path.join(tdir, 'alexandria.c')
                objectfile = os.path.join(d, 'alexandria.' + object_suffix)
                impfile = os.path.join(d, 'alexandria.lib')
                if cc.get_argument_syntax() == 'msvc':
                    shlibfile = os.path.join(d, 'alexandria.' + shared_suffix)
                    linkfile = impfile  # MSVC links against the *.lib instead of the *.dll
                elif is_cygwin():
                    shlibfile = os.path.join(d, 'cygalexandria.' + shared_suffix)
                    linkfile = shlibfile
                else:
                    shlibfile = os.path.join(d, 'libalexandria.' + shared_suffix)
                    linkfile = shlibfile
                # Ensure MSVC extra files end up in the directory that gets deleted
                # at the end
                with chdir(d):
                    self.build_shared_lib(cc, source, objectfile, shlibfile, impfile)

                with open(os.path.join(d, 'alexandria.pc'), 'w',
                          encoding='utf-8') as f:
                    f.write(textwrap.dedent('''
                        Name: alexandria
                        Description: alexandria
                        Version: 1.0.0
                        Libs: {}
                        ''').format(
                            Path(linkfile).as_posix().replace(' ', r'\ '),
                        ))

                # Run the test
                self.init(tdir, override_envvars={'PKG_CONFIG_PATH': d},
                        extra_args=['-Dmethod=pkg-config'])
                self.build()
                self.run_tests()

                self.wipe()

    @skip_if_no_cmake
    def test_prebuilt_shared_lib_cmake(self) -> None:
        (cc, _, object_suffix, shared_suffixes) = self.detect_prebuild_env_versioned()
        tdir = os.path.join(self.unit_test_dir, '17 prebuilt shared')
        for shared_suffix in shared_suffixes:
            with tempfile.TemporaryDirectory() as d:
                source = os.path.join(tdir, 'alexandria.c')
                objectfile = os.path.join(d, 'alexandria.' + object_suffix)
                impfile = os.path.join(d, 'alexandria.lib')
                if cc.get_argument_syntax() == 'msvc':
                    shlibfile = os.path.join(d, 'alexandria.' + shared_suffix)
                    linkfile = impfile  # MSVC links against the *.lib instead of the *.dll
                elif is_cygwin():
                    shlibfile = os.path.join(d, 'cygalexandria.' + shared_suffix)
                    linkfile = shlibfile
                else:
                    shlibfile = os.path.join(d, 'libalexandria.' + shared_suffix)
                    linkfile = shlibfile
                # Ensure MSVC extra files end up in the directory that gets deleted
                # at the end
                with chdir(d):
                    self.build_shared_lib(cc, source, objectfile, shlibfile, impfile)

                with open(os.path.join(d, 'alexandriaConfig.cmake'), 'w',
                        encoding='utf-8') as f:
                    f.write(textwrap.dedent('''
                        set(alexandria_FOUND ON)
                        set(alexandria_LIBRARIES "{}")
                        set(alexandria_INCLUDE_DIRS "{}")
                        ''').format(
                            re.sub(r'([\\"])', r'\\\1', linkfile),
                            re.sub(r'([\\"])', r'\\\1', tdir),
                        ))

                # Run the test
                self.init(tdir, override_envvars={'CMAKE_PREFIX_PATH': d},
                        extra_args=['-Dmethod=cmake'])
                self.build()
                self.run_tests()

                self.wipe()

    def test_prebuilt_shared_lib_rpath_same_prefix(self) -> None:
        (cc, _, object_suffix, shared_suffix) = self.detect_prebuild_env()
        orig_tdir = os.path.join(self.unit_test_dir, '17 prebuilt shared')

        # Put the shared library in a location that shares a common prefix with
        # the source directory:
        #
        #   .../
        #       foo-lib/
        #               libalexandria.so
        #       foo/
        #           meson.build
        #           ...
        #
        # This allows us to check that the .../foo-lib/libalexandria.so path is
        # preserved correctly when meson processes it.
        with tempfile.TemporaryDirectory() as d:
            libdir = os.path.join(d, 'foo-lib')
            os.mkdir(libdir)

            source = os.path.join(orig_tdir, 'alexandria.c')
            objectfile = os.path.join(libdir, 'alexandria.' + object_suffix)
            impfile = os.path.join(libdir, 'alexandria.lib')
            if cc.get_argument_syntax() == 'msvc':
                shlibfile = os.path.join(libdir, 'alexandria.' + shared_suffix)
            elif is_cygwin():
                shlibfile = os.path.join(libdir, 'cygalexandria.' + shared_suffix)
            else:
                shlibfile = os.path.join(libdir, 'libalexandria.' + shared_suffix)
            # Ensure MSVC extra files end up in the directory that gets deleted
            # at the end
            with chdir(libdir):
                self.build_shared_lib(cc, source, objectfile, shlibfile, impfile)

            tdir = os.path.join(d, 'foo')
            shutil.copytree(orig_tdir, tdir)

            # Run the test
            self.init(tdir, extra_args=[f'-Dsearch_dir={libdir}'])
            self.build()
            self.run_tests()

    def test_underscore_prefix_detection_list(self) -> None:
        '''
        Test the underscore detection hardcoded lookup list
        against what was detected in the binary.
        '''
        env, cc = get_convincing_fake_env_and_cc(self.builddir, self.prefix)
        expected_uscore = cc._symbols_have_underscore_prefix_searchbin(env)
        list_uscore = cc._symbols_have_underscore_prefix_list(env)
        if list_uscore is not None:
            self.assertEqual(list_uscore, expected_uscore)
        else:
            raise SkipTest('No match in underscore prefix list for this platform.')

    def test_underscore_prefix_detection_define(self) -> None:
        '''
        Test the underscore detection based on compiler-defined preprocessor macro
        against what was detected in the binary.
        '''
        env, cc = get_convincing_fake_env_and_cc(self.builddir, self.prefix)
        expected_uscore = cc._symbols_have_underscore_prefix_searchbin(env)
        define_uscore = cc._symbols_have_underscore_prefix_define(env)
        if define_uscore is not None:
            self.assertEqual(define_uscore, expected_uscore)
        else:
            raise SkipTest('Did not find the underscore prefix define __USER_LABEL_PREFIX__')

    @skipIfNoPkgconfig
    def test_pkgconfig_static(self):
        '''
        Test that the we prefer static libraries when `static: true` is
        passed to dependency() with pkg-config. Can't be an ordinary test
        because we need to build libs and try to find them from meson.build

        Also test that it's not a hard error to have unsatisfiable library deps
        since system libraries -lm will never be found statically.
        https://github.com/mesonbuild/meson/issues/2785
        '''
        (cc, stlinker, objext, shext) = self.detect_prebuild_env()
        testdir = os.path.join(self.unit_test_dir, '18 pkgconfig static')
        source = os.path.join(testdir, 'foo.c')
        objectfile = os.path.join(testdir, 'foo.' + objext)
        stlibfile = os.path.join(testdir, 'libfoo.a')
        impfile = os.path.join(testdir, 'foo.lib')
        if cc.get_argument_syntax() == 'msvc':
            shlibfile = os.path.join(testdir, 'foo.' + shext)
        elif is_cygwin():
            shlibfile = os.path.join(testdir, 'cygfoo.' + shext)
        else:
            shlibfile = os.path.join(testdir, 'libfoo.' + shext)
        # Build libs
        self.build_static_lib(cc, stlinker, source, objectfile, stlibfile, extra_args=['-DFOO_STATIC'])
        self.build_shared_lib(cc, source, objectfile, shlibfile, impfile)
        # Run test
        try:
            self.init(testdir, override_envvars={'PKG_CONFIG_LIBDIR': self.builddir})
            self.build()
            self.run_tests()
        finally:
            os.unlink(stlibfile)
            os.unlink(shlibfile)
            if is_windows():
                # Clean up all the garbage MSVC writes in the
                # source tree.
                for fname in glob(os.path.join(testdir, 'foo.*')):
                    if os.path.splitext(fname)[1] not in ['.c', '.h', '.in']:
                        os.unlink(fname)

    @skipIfNoPkgconfig
    @mock.patch.dict(os.environ)
    def test_pkgconfig_gen_escaping(self):
        testdir = os.path.join(self.common_test_dir, '44 pkgconfig-gen')
        prefix = '/usr/with spaces'
        libdir = 'lib'
        self.init(testdir, extra_args=['--prefix=' + prefix,
                                       '--libdir=' + libdir])
        # Find foo dependency
        os.environ['PKG_CONFIG_LIBDIR'] = self.privatedir
        env = get_fake_env(testdir, self.builddir, self.prefix)
        kwargs = {'required': True, 'silent': True}
        foo_dep = PkgConfigDependency('libanswer', env, kwargs)
        # Ensure link_args are properly quoted
        libdir = PurePath(prefix) / PurePath(libdir)
        link_args = ['-L' + libdir.as_posix(), '-lanswer']
        self.assertEqual(foo_dep.get_link_args(), link_args)
        # Ensure include args are properly quoted
        incdir = PurePath(prefix) / PurePath('include')
        cargs = ['-I' + incdir.as_posix(), '-DLIBFOO']
        # pkg-config and pkgconf does not respect the same order
        self.assertEqual(sorted(foo_dep.get_compile_args()), sorted(cargs))

    @skipIfNoPkgconfig
    def test_pkgconfig_relocatable(self):
        '''
        Test that it generates relocatable pkgconfig when module
        option pkgconfig.relocatable=true.
        '''
        testdir_rel = os.path.join(self.common_test_dir, '44 pkgconfig-gen')
        self.init(testdir_rel, extra_args=['-Dpkgconfig.relocatable=true'])

        def check_pcfile(name, *, relocatable, levels=2):
            with open(os.path.join(self.privatedir, name), encoding='utf-8') as f:
                pcfile = f.read()
                # The pkgconfig module always uses posix path regardless of platform
                prefix_rel = PurePath('${pcfiledir}', *(['..'] * levels)).as_posix()
                (self.assertIn if relocatable else self.assertNotIn)(
                    f'prefix={prefix_rel}\n',
                    pcfile)

        check_pcfile('libvartest.pc', relocatable=True)
        check_pcfile('libvartest2.pc', relocatable=True)

        self.wipe()
        self.init(testdir_rel, extra_args=['-Dpkgconfig.relocatable=false'])

        check_pcfile('libvartest.pc', relocatable=False)
        check_pcfile('libvartest2.pc', relocatable=False)

        self.wipe()
        testdir_abs = os.path.join(self.unit_test_dir, '106 pkgconfig relocatable with absolute path')
        self.init(testdir_abs)

        check_pcfile('libsimple.pc', relocatable=True, levels=3)

    def test_array_option_change(self):
        def get_opt():
            opts = self.introspect('--buildoptions')
            for x in opts:
                if x.get('name') == 'list':
                    return x
            raise Exception(opts)

        expected = {
            'name': 'list',
            'description': 'list',
            'section': 'user',
            'type': 'array',
            'value': ['foo', 'bar'],
            'choices': ['foo', 'bar', 'oink', 'boink'],
            'machine': 'any',
        }
        tdir = os.path.join(self.unit_test_dir, '19 array option')
        self.init(tdir)
        original = get_opt()
        self.assertDictEqual(original, expected)

        expected['value'] = ['oink', 'boink']
        self.setconf('-Dlist=oink,boink')
        changed = get_opt()
        self.assertEqual(changed, expected)

    def test_array_option_bad_change(self):
        def get_opt():
            opts = self.introspect('--buildoptions')
            for x in opts:
                if x.get('name') == 'list':
                    return x
            raise Exception(opts)

        expected = {
            'name': 'list',
            'description': 'list',
            'section': 'user',
            'type': 'array',
            'value': ['foo', 'bar'],
            'choices': ['foo', 'bar', 'oink', 'boink'],
            'machine': 'any',
        }
        tdir = os.path.join(self.unit_test_dir, '19 array option')
        self.init(tdir)
        original = get_opt()
        self.assertDictEqual(original, expected)
        with self.assertRaises(subprocess.CalledProcessError):
            self.setconf('-Dlist=bad')
        changed = get_opt()
        self.assertDictEqual(changed, expected)

    def test_array_option_empty_equivalents(self):
        """Array options treat -Dopt=[] and -Dopt= as equivalent."""
        def get_opt():
            opts = self.introspect('--buildoptions')
            for x in opts:
                if x.get('name') == 'list':
                    return x
            raise Exception(opts)

        expected = {
            'name': 'list',
            'description': 'list',
            'section': 'user',
            'type': 'array',
            'value': [],
            'choices': ['foo', 'bar', 'oink', 'boink'],
            'machine': 'any',
        }
        tdir = os.path.join(self.unit_test_dir, '19 array option')
        self.init(tdir, extra_args='-Dlist=')
        original = get_opt()
        self.assertDictEqual(original, expected)

    def test_executable_names(self):
        testdir = os.path.join(self.unit_test_dir, '121 executable suffix')
        self.init(testdir)
        self.build()
        exe1 = os.path.join(self.builddir, 'foo' + exe_suffix)
        exe2 = os.path.join(self.builddir, 'foo.bin')
        self.assertPathExists(exe1)
        self.assertPathExists(exe2)
        self.assertNotEqual(exe1, exe2)

        # Wipe and run the compile command against the target names
        self.init(testdir, extra_args=['--wipe'])
        self._run([*self.meson_command, 'compile', '-C', self.builddir, './foo'])
        self._run([*self.meson_command, 'compile', '-C', self.builddir, './foo.bin'])
        self.assertPathExists(exe1)
        self.assertPathExists(exe2)
        self.assertNotEqual(exe1, exe2)


    def opt_has(self, name, value):
        res = self.introspect('--buildoptions')
        found = False
        for i in res:
            if i['name'] == name:
                self.assertEqual(i['value'], value)
                found = True
                break
        self.assertTrue(found, "Array option not found in introspect data.")

    def test_free_stringarray_setting(self):
        testdir = os.path.join(self.common_test_dir, '40 options')
        self.init(testdir)
        self.opt_has('free_array_opt', [])
        self.setconf('-Dfree_array_opt=foo,bar', will_build=False)
        self.opt_has('free_array_opt', ['foo', 'bar'])
        self.setconf("-Dfree_array_opt=['a,b', 'c,d']", will_build=False)
        self.opt_has('free_array_opt', ['a,b', 'c,d'])

    # When running under Travis Mac CI, the file updates seem to happen
    # too fast so the timestamps do not get properly updated.
    # Call this method before file operations in appropriate places
    # to make things work.
    def mac_ci_delay(self):
        if is_osx() and is_ci():
            import time
            time.sleep(1)

    def test_options_with_choices_changing(self) -> None:
        """Detect when options like arrays or combos have their choices change."""
        testdir = Path(os.path.join(self.unit_test_dir, '83 change option choices'))
        options1 = str(testdir / 'meson_options.1.txt')
        options2 = str(testdir / 'meson_options.2.txt')

        # Test that old options are changed to the new defaults if they are not valid
        real_options = str(testdir / 'meson_options.txt')
        self.addCleanup(os.unlink, real_options)

        shutil.copy(options1, real_options)
        self.init(str(testdir))
        self.mac_ci_delay()
        shutil.copy(options2, real_options)

        self.build()
        opts = self.introspect('--buildoptions')
        for item in opts:
            if item['name'] == 'combo':
                self.assertEqual(item['value'], 'b')
                self.assertEqual(item['choices'], ['b', 'c', 'd'])
            elif item['name'] == 'array':
                self.assertEqual(item['value'], ['b'])
                self.assertEqual(item['choices'], ['b', 'c', 'd'])

        self.wipe()
        self.mac_ci_delay()

        # When the old options are valid they should remain
        shutil.copy(options1, real_options)
        self.init(str(testdir), extra_args=['-Dcombo=c', '-Darray=b,c'])
        self.mac_ci_delay()
        shutil.copy(options2, real_options)
        self.build()
        opts = self.introspect('--buildoptions')
        for item in opts:
            if item['name'] == 'combo':
                self.assertEqual(item['value'], 'c')
                self.assertEqual(item['choices'], ['b', 'c', 'd'])
            elif item['name'] == 'array':
                self.assertEqual(item['value'], ['b', 'c'])
                self.assertEqual(item['choices'], ['b', 'c', 'd'])

    def test_options_listed_in_build_options(self) -> None:
        """Detect when changed options become listed in build options."""
        testdir = os.path.join(self.unit_test_dir, '113 list build options')

        out = self.init(testdir)
        for line in out.splitlines():
            if line.startswith('Message: Build options:'):
                self.assertNotIn('-Dauto_features=auto', line)
                self.assertNotIn('-Doptional=auto', line)

        self.wipe()
        self.mac_ci_delay()

        out = self.init(testdir, extra_args=['-Dauto_features=disabled', '-Doptional=enabled'])
        for line in out.splitlines():
            if line.startswith('Message: Build options:'):
                self.assertIn('-Dauto_features=disabled', line)
                self.assertIn('-Doptional=enabled', line)

        self.setconf('-Doptional=disabled')
        out = self.build()
        for line in out.splitlines():
            if line.startswith('Message: Build options:'):
                self.assertIn('-Dauto_features=disabled', line)
                self.assertNotIn('-Doptional=enabled', line)
                self.assertIn('-Doptional=disabled', line)

    def test_subproject_promotion(self):
        testdir = os.path.join(self.unit_test_dir, '12 promote')
        workdir = os.path.join(self.builddir, 'work')
        shutil.copytree(testdir, workdir)
        spdir = os.path.join(workdir, 'subprojects')
        s3dir = os.path.join(spdir, 's3')
        scommondir = os.path.join(spdir, 'scommon')
        self.assertFalse(os.path.isdir(s3dir))
        subprocess.check_call(self.wrap_command + ['promote', 's3'],
                              cwd=workdir,
                              stdout=subprocess.DEVNULL)
        self.assertTrue(os.path.isdir(s3dir))
        self.assertFalse(os.path.isdir(scommondir))
        self.assertNotEqual(subprocess.call(self.wrap_command + ['promote', 'scommon'],
                                            cwd=workdir,
                                            stderr=subprocess.DEVNULL), 0)
        self.assertNotEqual(subprocess.call(self.wrap_command + ['promote', 'invalid/path/to/scommon'],
                                            cwd=workdir,
                                            stderr=subprocess.DEVNULL), 0)
        self.assertFalse(os.path.isdir(scommondir))
        subprocess.check_call(self.wrap_command + ['promote', 'subprojects/s2/subprojects/scommon'], cwd=workdir)
        self.assertTrue(os.path.isdir(scommondir))
        promoted_wrap = os.path.join(spdir, 'athing.wrap')
        self.assertFalse(os.path.isfile(promoted_wrap))
        subprocess.check_call(self.wrap_command + ['promote', 'athing'], cwd=workdir)
        self.assertTrue(os.path.isfile(promoted_wrap))
        self.new_builddir()  # Ensure builddir is not parent or workdir
        self.init(workdir)
        self.build()

    def test_subproject_promotion_wrap(self):
        testdir = os.path.join(self.unit_test_dir, '43 promote wrap')
        workdir = os.path.join(self.builddir, 'work')
        shutil.copytree(testdir, workdir)
        spdir = os.path.join(workdir, 'subprojects')

        ambiguous_wrap = os.path.join(spdir, 'ambiguous.wrap')
        self.assertNotEqual(subprocess.call(self.wrap_command + ['promote', 'ambiguous'],
                                            cwd=workdir,
                                            stderr=subprocess.DEVNULL), 0)
        self.assertFalse(os.path.isfile(ambiguous_wrap))
        subprocess.check_call(self.wrap_command + ['promote', 'subprojects/s2/subprojects/ambiguous.wrap'], cwd=workdir)
        self.assertTrue(os.path.isfile(ambiguous_wrap))

    def test_warning_location(self):
        tdir = os.path.join(self.unit_test_dir, '22 warning location')
        out = self.init(tdir)
        for expected in [
            r'meson.build:4: WARNING: Keyword argument "link_with" defined multiple times.',
            r'sub' + os.path.sep + r'meson.build:3: WARNING: Keyword argument "link_with" defined multiple times.',
            r'meson.build:6: WARNING: a warning of some sort',
            r'sub' + os.path.sep + r'meson.build:4: WARNING: subdir warning',
            r'meson.build:7: WARNING: Module SIMD has no backwards or forwards compatibility and might not exist in future releases.',
            r"meson.build:11: WARNING: The variable(s) 'MISSING' in the input file 'conf.in' are not present in the given configuration data.",
        ]:
            with self.subTest(expected):
                self.assertRegex(out, re.escape(expected))

        for wd in [
            self.src_root,
            self.builddir,
            os.getcwd(),
        ]:
            with self.subTest(wd):
                self.new_builddir()
                out = self.init(tdir, workdir=wd)
                expected = os.path.join(relpath(tdir, self.src_root), 'meson.build')
                relwd = relpath(self.src_root, wd)
                if relwd != '.':
                    expected = os.path.join(relwd, expected)
                    expected = '\n' + expected + ':'
                self.assertIn(expected, out)

    def test_error_location_path(self):
        '''Test locations in meson errors contain correct paths'''
        # this list contains errors from all the different steps in the
        # lexer/parser/interpreter we have tests for.
        for (t, f) in [
            ('10 out of bounds', 'meson.build'),
            ('18 wrong plusassign', 'meson.build'),
            ('56 bad option argument', 'meson_options.txt'),
            ('94 subdir parse error', os.path.join('subdir', 'meson.build')),
            ('95 invalid option file', 'meson_options.txt'),
        ]:
            tdir = os.path.join(self.src_root, 'test cases', 'failing', t)

            for wd in [
                self.src_root,
                self.builddir,
                os.getcwd(),
            ]:
                try:
                    self.init(tdir, workdir=wd)
                except subprocess.CalledProcessError as e:
                    expected = os.path.join('test cases', 'failing', t, f)
                    relwd = relpath(self.src_root, wd)
                    if relwd != '.':
                        expected = os.path.join(relwd, expected)
                    expected = '\n' + expected + ':'
                    self.assertIn(expected, e.output)
                else:
                    self.fail('configure unexpectedly succeeded')

    def test_permitted_method_kwargs(self):
        tdir = os.path.join(self.unit_test_dir, '25 non-permitted kwargs')
        with self.assertRaises(subprocess.CalledProcessError) as cm:
            self.init(tdir)
        self.assertIn('ERROR: compiler.has_header_symbol got unknown keyword arguments "prefixxx"', cm.exception.output)

    def test_templates(self):
        ninja = mesonbuild.environment.detect_ninja()
        if ninja is None:
            raise SkipTest('This test currently requires ninja. Fix this once "meson build" works.')

        langs = ['c']
        env = get_fake_env()
        for l in ['cpp', 'cs', 'd', 'java', 'cuda', 'fortran', 'objc', 'objcpp', 'rust', 'vala']:
            try:
                comp = detect_compiler_for(env, l, MachineChoice.HOST, True, '')
                with tempfile.TemporaryDirectory() as d:
                    comp.sanity_check(d, env)
                langs.append(l)
            except EnvironmentException:
                pass

        # The D template fails under mac CI and we don't know why.
        # Patches welcome
        if is_osx():
            langs = [l for l in langs if l != 'd']

        for lang in langs:
            for target_type in ('executable', 'library'):
                with self.subTest(f'Language: {lang}; type: {target_type}'):
                    if is_windows() and lang == 'fortran' and target_type == 'library':
                        # non-Gfortran Windows Fortran compilers do not do shared libraries in a Fortran standard way
                        # see "test cases/fortran/6 dynamic"
                        fc = detect_compiler_for(env, 'fortran', MachineChoice.HOST, True, '')
                        if fc.get_id() in {'intel-cl', 'pgi'}:
                            continue
                    # test empty directory
                    with tempfile.TemporaryDirectory() as tmpdir:
                        self._run(self.meson_command + ['init', '--language', lang, '--type', target_type],
                                  workdir=tmpdir)
                        self._run(self.setup_command + ['--backend=ninja', 'builddir'],
                                  workdir=tmpdir)
                        self._run(ninja,
                                  workdir=os.path.join(tmpdir, 'builddir'))
                # test directory with existing code file
                if lang in {'c', 'cpp', 'd'}:
                    with tempfile.TemporaryDirectory() as tmpdir:
                        with open(os.path.join(tmpdir, 'foo.' + lang), 'w', encoding='utf-8') as f:
                            f.write('int main(void) {}')
                        self._run(self.meson_command + ['init', '-b'], workdir=tmpdir)
                elif lang in {'java'}:
                    with tempfile.TemporaryDirectory() as tmpdir:
                        with open(os.path.join(tmpdir, 'Foo.' + lang), 'w', encoding='utf-8') as f:
                            f.write('public class Foo { public static void main() {} }')
                        self._run(self.meson_command + ['init', '-b'], workdir=tmpdir)

    def test_compiler_run_command(self):
        '''
        The test checks that the compiler object can be passed to
        run_command().
        '''
        testdir = os.path.join(self.unit_test_dir, '24 compiler run_command')
        self.init(testdir)

    def test_identical_target_name_in_subproject_flat_layout(self):
        '''
        Test that identical targets in different subprojects do not collide
        if layout is flat.
        '''
        testdir = os.path.join(self.common_test_dir, '172 identical target name in subproject flat layout')
        self.init(testdir, extra_args=['--layout=flat'])
        self.build()

    def test_identical_target_name_in_subdir_flat_layout(self):
        '''
        Test that identical targets in different subdirs do not collide
        if layout is flat.
        '''
        testdir = os.path.join(self.common_test_dir, '181 same target name flat layout')
        self.init(testdir, extra_args=['--layout=flat'])
        self.build()

    def test_flock(self):
        exception_raised = False
        with tempfile.TemporaryDirectory() as tdir:
            os.mkdir(os.path.join(tdir, 'meson-private'))
            with BuildDirLock(tdir):
                try:
                    with BuildDirLock(tdir):
                        pass
                except MesonException:
                    exception_raised = True
        self.assertTrue(exception_raised, 'Double locking did not raise exception.')

    @skipIf(is_osx(), 'Test not applicable to OSX')
    def test_check_module_linking(self):
        """
        Test that link_with: a shared module issues a warning
        https://github.com/mesonbuild/meson/issues/2865
        (That an error is raised on OSX is exercised by test failing/78)
        """
        tdir = os.path.join(self.unit_test_dir, '30 shared_mod linking')
        out = self.init(tdir)
        msg = ('''DEPRECATION: target prog links against shared module mymod, which is incorrect.
             This will be an error in the future, so please use shared_library() for mymod instead.
             If shared_module() was used for mymod because it has references to undefined symbols,
             use shared_library() with `override_options: ['b_lundef=false']` instead.''')
        self.assertIn(msg, out)

    def test_mixed_language_linker_check(self):
        testdir = os.path.join(self.unit_test_dir, '97 compiler.links file arg')
        self.init(testdir)
        cmds = self.get_meson_log_compiler_checks()
        self.assertEqual(len(cmds), 5)
        # Path to the compilers, gleaned from cc.compiles tests
        cc = cmds[0][0]
        cxx = cmds[1][0]
        # cc.links
        self.assertEqual(cmds[2][0], cc)
        # cxx.links with C source
        self.assertEqual(cmds[3][0], cc)
        self.assertEqual(cmds[4][0], cxx)
        if self.backend is Backend.ninja:
            # updating the file to check causes a reconfigure
            #
            # only the ninja backend is competent enough to detect reconfigured
            # no-op builds without build targets
            self.utime(os.path.join(testdir, 'test.c'))
            self.assertReconfiguredBuildIsNoop()

    def test_ndebug_if_release_disabled(self):
        testdir = os.path.join(self.unit_test_dir, '28 ndebug if-release')
        self.init(testdir, extra_args=['--buildtype=release', '-Db_ndebug=if-release'])
        self.build()
        exe = os.path.join(self.builddir, 'main')
        self.assertEqual(b'NDEBUG=1', subprocess.check_output(exe).strip())

    def test_ndebug_if_release_enabled(self):
        testdir = os.path.join(self.unit_test_dir, '28 ndebug if-release')
        self.init(testdir, extra_args=['--buildtype=debugoptimized', '-Db_ndebug=if-release'])
        self.build()
        exe = os.path.join(self.builddir, 'main')
        self.assertEqual(b'NDEBUG=0', subprocess.check_output(exe).strip())

    def test_guessed_linker_dependencies(self):
        '''
        Test that meson adds dependencies for libraries based on the final
        linker command line.
        '''
        testdirbase = os.path.join(self.unit_test_dir, '29 guessed linker dependencies')
        testdirlib = os.path.join(testdirbase, 'lib')

        extra_args = None
        libdir_flags = ['-L']
        env = get_fake_env(testdirlib, self.builddir, self.prefix)
        if detect_c_compiler(env, MachineChoice.HOST).get_id() in {'msvc', 'clang-cl', 'intel-cl'}:
            # msvc-like compiler, also test it with msvc-specific flags
            libdir_flags += ['/LIBPATH:', '-LIBPATH:']
        else:
            # static libraries are not linkable with -l with msvc because meson installs them
            # as .a files which unix_args_to_native will not know as it expects libraries to use
            # .lib as extension. For a DLL the import library is installed as .lib. Thus for msvc
            # this tests needs to use shared libraries to test the path resolving logic in the
            # dependency generation code path.
            extra_args = ['--default-library', 'static']

        initial_builddir = self.builddir
        initial_installdir = self.installdir

        for libdir_flag in libdir_flags:
            # build library
            self.new_builddir()
            self.init(testdirlib, extra_args=extra_args)
            self.build()
            self.install()
            libbuilddir = self.builddir
            installdir = self.installdir
            libdir = os.path.join(self.installdir, self.prefix.lstrip('/').lstrip('\\'), 'lib')

            # build user of library
            self.new_builddir()
            # replace is needed because meson mangles platform paths passed via LDFLAGS
            self.init(os.path.join(testdirbase, 'exe'),
                      override_envvars={"LDFLAGS": '{}{}'.format(libdir_flag, libdir.replace('\\', '/'))})
            self.build()
            self.assertBuildIsNoop()

            # rebuild library
            exebuilddir = self.builddir
            self.installdir = installdir
            self.builddir = libbuilddir
            # Microsoft's compiler is quite smart about touching import libs on changes,
            # so ensure that there is actually a change in symbols.
            self.setconf('-Dmore_exports=true')
            self.build()
            self.install()
            # no ensure_backend_detects_changes needed because self.setconf did that already

            # assert user of library will be rebuild
            self.builddir = exebuilddir
            self.assertRebuiltTarget('app')

            # restore dirs for the next test case
            self.installdir = initial_builddir
            self.builddir = initial_installdir

    def test_conflicting_d_dash_option(self):
        testdir = os.path.join(self.unit_test_dir, '37 mixed command line args')
        with self.assertRaises((subprocess.CalledProcessError, RuntimeError)) as e:
            self.init(testdir, extra_args=['-Dbindir=foo', '--bindir=bar'])
            # Just to ensure that we caught the correct error
            self.assertIn('as both', e.stderr)

    def _test_same_option_twice(self, arg, args):
        testdir = os.path.join(self.unit_test_dir, '37 mixed command line args')
        self.init(testdir, extra_args=args)
        opts = self.introspect('--buildoptions')
        for item in opts:
            if item['name'] == arg:
                self.assertEqual(item['value'], 'bar')
                return
        raise Exception(f'Missing {arg} value?')

    def test_same_dash_option_twice(self):
        self._test_same_option_twice('bindir', ['--bindir=foo', '--bindir=bar'])

    def test_same_d_option_twice(self):
        self._test_same_option_twice('bindir', ['-Dbindir=foo', '-Dbindir=bar'])

    def test_same_project_d_option_twice(self):
        self._test_same_option_twice('one', ['-Done=foo', '-Done=bar'])

    def _test_same_option_twice_configure(self, arg, args):
        testdir = os.path.join(self.unit_test_dir, '37 mixed command line args')
        self.init(testdir)
        self.setconf(args)
        opts = self.introspect('--buildoptions')
        for item in opts:
            if item['name'] == arg:
                self.assertEqual(item['value'], 'bar')
                return
        raise Exception(f'Missing {arg} value?')

    def test_same_dash_option_twice_configure(self):
        self._test_same_option_twice_configure(
            'bindir', ['--bindir=foo', '--bindir=bar'])

    def test_same_d_option_twice_configure(self):
        self._test_same_option_twice_configure(
            'bindir', ['-Dbindir=foo', '-Dbindir=bar'])

    def test_same_project_d_option_twice_configure(self):
        self._test_same_option_twice_configure(
            'one', ['-Done=foo', '-Done=bar'])

    def test_command_line(self):
        testdir = os.path.join(self.unit_test_dir, '34 command line')

        # Verify default values when passing no args that affect the
        # configuration, and as a bonus, test that --profile-self works.
        out = self.init(testdir, extra_args=['--profile-self', '--fatal-meson-warnings'])
        self.assertNotIn('[default: true]', out)
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('default_library')].value, 'static')
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '1')
        self.assertEqual(obj.options[OptionKey('set_sub_opt')].value, True)
        self.assertEqual(obj.options[OptionKey('subp_opt', 'subp')].value, 'default3')
        self.wipe()

        # warning_level is special, it's --warnlevel instead of --warning-level
        # for historical reasons
        self.init(testdir, extra_args=['--warnlevel=2', '--fatal-meson-warnings'])
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '2')
        self.setconf('--warnlevel=3')
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '3')
        self.setconf('--warnlevel=everything')
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, 'everything')
        self.wipe()

        # But when using -D syntax, it should be 'warning_level'
        self.init(testdir, extra_args=['-Dwarning_level=2', '--fatal-meson-warnings'])
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '2')
        self.setconf('-Dwarning_level=3')
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '3')
        self.setconf('-Dwarning_level=everything')
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, 'everything')
        self.wipe()

        # Mixing --option and -Doption is forbidden
        with self.assertRaises((subprocess.CalledProcessError, RuntimeError)) as cm:
            self.init(testdir, extra_args=['--warnlevel=1', '-Dwarning_level=3'])
            if isinstance(cm.exception, subprocess.CalledProcessError):
                self.assertNotEqual(0, cm.exception.returncode)
                self.assertIn('as both', cm.exception.output)
            else:
                self.assertIn('as both', str(cm.exception))
        self.init(testdir)
        with self.assertRaises((subprocess.CalledProcessError, RuntimeError)) as cm:
            self.setconf(['--warnlevel=1', '-Dwarning_level=3'])
            if isinstance(cm.exception, subprocess.CalledProcessError):
                self.assertNotEqual(0, cm.exception.returncode)
                self.assertIn('as both', cm.exception.output)
            else:
                self.assertIn('as both', str(cm.exception))
        self.wipe()

        # --default-library should override default value from project()
        self.init(testdir, extra_args=['--default-library=both', '--fatal-meson-warnings'])
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('default_library')].value, 'both')
        self.setconf('--default-library=shared')
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('default_library')].value, 'shared')
        if self.backend is Backend.ninja:
            # reconfigure target works only with ninja backend
            self.build('reconfigure')
            obj = mesonbuild.coredata.load(self.builddir)
            self.assertEqual(obj.options[OptionKey('default_library')].value, 'shared')
        self.wipe()

        # Should fail on unknown options
        with self.assertRaises((subprocess.CalledProcessError, RuntimeError)) as cm:
            self.init(testdir, extra_args=['-Dbad=1', '-Dfoo=2', '-Dwrong_link_args=foo'])
            self.assertNotEqual(0, cm.exception.returncode)
            self.assertIn(msg, cm.exception.output)
        self.wipe()

        # Should fail on malformed option
        msg = "Option 'foo' must have a value separated by equals sign."
        with self.assertRaises((subprocess.CalledProcessError, RuntimeError)) as cm:
            self.init(testdir, extra_args=['-Dfoo'])
            if isinstance(cm.exception, subprocess.CalledProcessError):
                self.assertNotEqual(0, cm.exception.returncode)
                self.assertIn(msg, cm.exception.output)
            else:
                self.assertIn(msg, str(cm.exception))
        self.init(testdir)
        with self.assertRaises((subprocess.CalledProcessError, RuntimeError)) as cm:
            self.setconf('-Dfoo')
            if isinstance(cm.exception, subprocess.CalledProcessError):
                self.assertNotEqual(0, cm.exception.returncode)
                self.assertIn(msg, cm.exception.output)
            else:
                self.assertIn(msg, str(cm.exception))
        self.wipe()

        # It is not an error to set wrong option for unknown subprojects or
        # language because we don't have control on which one will be selected.
        self.init(testdir, extra_args=['-Dc_wrong=1', '-Dwrong:bad=1'])
        self.wipe()

        # Test we can set subproject option
        self.init(testdir, extra_args=['-Dsubp:subp_opt=foo', '--fatal-meson-warnings'])
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('subp_opt', 'subp')].value, 'foo')
        self.wipe()

        # c_args value should be parsed with split_args
        self.init(testdir, extra_args=['-Dc_args=-Dfoo -Dbar "-Dthird=one two"', '--fatal-meson-warnings'])
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('args', lang='c')].value, ['-Dfoo', '-Dbar', '-Dthird=one two'])

        self.setconf('-Dc_args="foo bar" one two')
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('args', lang='c')].value, ['foo bar', 'one', 'two'])
        self.wipe()

        self.init(testdir, extra_args=['-Dset_percent_opt=myoption%', '--fatal-meson-warnings'])
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('set_percent_opt')].value, 'myoption%')
        self.wipe()

        # Setting a 2nd time the same option should override the first value
        try:
            self.init(testdir, extra_args=['--bindir=foo', '--bindir=bar',
                                           '-Dbuildtype=plain', '-Dbuildtype=release',
                                           '-Db_sanitize=address', '-Db_sanitize=thread',
                                           '-Dc_args=-Dfoo', '-Dc_args=-Dbar',
                                           '-Db_lundef=false', '--fatal-meson-warnings'])
            obj = mesonbuild.coredata.load(self.builddir)
            self.assertEqual(obj.options[OptionKey('bindir')].value, 'bar')
            self.assertEqual(obj.options[OptionKey('buildtype')].value, 'release')
            self.assertEqual(obj.options[OptionKey('b_sanitize')].value, 'thread')
            self.assertEqual(obj.options[OptionKey('args', lang='c')].value, ['-Dbar'])
            self.setconf(['--bindir=bar', '--bindir=foo',
                          '-Dbuildtype=release', '-Dbuildtype=plain',
                          '-Db_sanitize=thread', '-Db_sanitize=address',
                          '-Dc_args=-Dbar', '-Dc_args=-Dfoo'])
            obj = mesonbuild.coredata.load(self.builddir)
            self.assertEqual(obj.options[OptionKey('bindir')].value, 'foo')
            self.assertEqual(obj.options[OptionKey('buildtype')].value, 'plain')
            self.assertEqual(obj.options[OptionKey('b_sanitize')].value, 'address')
            self.assertEqual(obj.options[OptionKey('args', lang='c')].value, ['-Dfoo'])
            self.wipe()
        except KeyError:
            # Ignore KeyError, it happens on CI for compilers that does not
            # support b_sanitize. We have to test with a base option because
            # they used to fail this test with Meson 0.46 an earlier versions.
            pass

    def test_warning_level_0(self):
        testdir = os.path.join(self.common_test_dir, '207 warning level 0')

        # Verify default values when passing no args
        self.init(testdir)
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '0')
        self.wipe()

        # verify we can override w/ --warnlevel
        self.init(testdir, extra_args=['--warnlevel=1'])
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '1')
        self.setconf('--warnlevel=0')
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '0')
        self.wipe()

        # verify we can override w/ -Dwarning_level
        self.init(testdir, extra_args=['-Dwarning_level=1'])
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '1')
        self.setconf('-Dwarning_level=0')
        obj = mesonbuild.coredata.load(self.builddir)
        self.assertEqual(obj.options[OptionKey('warning_level')].value, '0')
        self.wipe()

    def test_feature_check_usage_subprojects(self):
        testdir = os.path.join(self.unit_test_dir, '40 featurenew subprojects')
        out = self.init(testdir)
        # Parent project warns correctly
        self.assertRegex(out, "WARNING: Project targets '>=0.45'.*'0.47.0': dict")
        # Subprojects warn correctly
        self.assertRegex(out, r"foo\| .*WARNING: Project targets '>=0.40'.*'0.44.0': disabler")
        self.assertRegex(out, r"baz\| .*WARNING: Project targets '!=0.40'.*'0.44.0': disabler")
        # Subproject has a new-enough meson_version, no warning
        self.assertNotRegex(out, "WARNING: Project targets.*Python")
        # Ensure a summary is printed in the subproject and the outer project
        self.assertRegex(out, r"\| WARNING: Project specifies a minimum meson_version '>=0.40'")
        self.assertRegex(out, r"\| \* 0.44.0: {'disabler'}")
        self.assertRegex(out, "WARNING: Project specifies a minimum meson_version '>=0.45'")
        self.assertRegex(out, " * 0.47.0: {'dict'}")

    def test_configure_file_warnings(self):
        testdir = os.path.join(self.common_test_dir, "14 configure file")
        out = self.init(testdir)
        self.assertRegex(out, "WARNING:.*'empty'.*config.h.in.*not present.*")
        self.assertRegex(out, "WARNING:.*'FOO_BAR'.*nosubst-nocopy2.txt.in.*not present.*")
        self.assertRegex(out, "WARNING:.*'empty'.*config.h.in.*not present.*")
        self.assertRegex(out, "WARNING:.*empty configuration_data.*test.py.in")
        # Warnings for configuration files that are overwritten.
        self.assertRegex(out, "WARNING:.*\"double_output.txt\".*overwrites")
        self.assertRegex(out, "WARNING:.*\"subdir.double_output2.txt\".*overwrites")
        self.assertNotRegex(out, "WARNING:.*no_write_conflict.txt.*overwrites")
        self.assertNotRegex(out, "WARNING:.*@BASENAME@.*overwrites")
        self.assertRegex(out, "WARNING:.*\"sameafterbasename\".*overwrites")
        # No warnings about empty configuration data objects passed to files with substitutions
        self.assertNotRegex(out, "WARNING:.*empty configuration_data.*nosubst-nocopy1.txt.in")
        self.assertNotRegex(out, "WARNING:.*empty configuration_data.*nosubst-nocopy2.txt.in")
        with open(os.path.join(self.builddir, 'nosubst-nocopy1.txt'), 'rb') as f:
            self.assertEqual(f.read().strip(), b'/* #undef FOO_BAR */')
        with open(os.path.join(self.builddir, 'nosubst-nocopy2.txt'), 'rb') as f:
            self.assertEqual(f.read().strip(), b'')

    def test_dirs(self):
        with tempfile.TemporaryDirectory() as containing:
            with tempfile.TemporaryDirectory(dir=containing) as srcdir:
                mfile = os.path.join(srcdir, 'meson.build')
                of = open(mfile, 'w', encoding='utf-8')
                of.write("project('foobar', 'c')\n")
                of.close()
                pc = subprocess.run(self.setup_command,
                                    cwd=srcdir,
                                    stdout=subprocess.PIPE,
                                    stderr=subprocess.DEVNULL)
                self.assertIn(b'Must specify at least one directory name', pc.stdout)
                with tempfile.TemporaryDirectory(dir=srcdir) as builddir:
                    subprocess.run(self.setup_command,
                                   check=True,
                                   cwd=builddir,
                                   stdout=subprocess.DEVNULL,
                                   stderr=subprocess.DEVNULL)

    def get_opts_as_dict(self):
        result = {}
        for i in self.introspect('--buildoptions'):
            result[i['name']] = i['value']
        return result

    def test_buildtype_setting(self):
        testdir = os.path.join(self.common_test_dir, '1 trivial')
        self.init(testdir)
        opts = self.get_opts_as_dict()
        self.assertEqual(opts['buildtype'], 'debug')
        self.assertEqual(opts['debug'], True)
        self.setconf('-Ddebug=false')
        opts = self.get_opts_as_dict()
        self.assertEqual(opts['debug'], False)
        self.assertEqual(opts['buildtype'], 'debug')
        self.assertEqual(opts['optimization'], '0')
        self.setconf('-Doptimization=g')
        opts = self.get_opts_as_dict()
        self.assertEqual(opts['debug'], False)
        self.assertEqual(opts['buildtype'], 'debug')
        self.assertEqual(opts['optimization'], 'g')

    @skipIfNoPkgconfig
    @skipIf(is_windows(), 'Help needed with fixing this test on windows')
    def test_native_dep_pkgconfig(self):
        testdir = os.path.join(self.unit_test_dir,
                               '45 native dep pkgconfig var')
        with tempfile.NamedTemporaryFile(mode='w', delete=False, encoding='utf-8') as crossfile:
            crossfile.write(textwrap.dedent(
                '''[binaries]
                pkg-config = '{}'

                [properties]

                [host_machine]
                system = 'linux'
                cpu_family = 'arm'
                cpu = 'armv7'
                endian = 'little'
                '''.format(os.path.join(testdir, 'cross_pkgconfig.py'))))
            crossfile.flush()
            self.meson_cross_files = [crossfile.name]

        env = {'PKG_CONFIG_LIBDIR':  os.path.join(testdir,
                                                  'native_pkgconfig')}
        self.init(testdir, extra_args=['-Dstart_native=false'], override_envvars=env)
        self.wipe()
        self.init(testdir, extra_args=['-Dstart_native=true'], override_envvars=env)

    @skipIfNoPkgconfig
    @skipIf(is_windows(), 'Help needed with fixing this test on windows')
    def test_pkg_config_libdir(self):
        testdir = os.path.join(self.unit_test_dir,
                               '45 native dep pkgconfig var')
        with tempfile.NamedTemporaryFile(mode='w', delete=False, encoding='utf-8') as crossfile:
            crossfile.write(textwrap.dedent(
                '''[binaries]
                pkg-config = 'pkg-config'

                [properties]
                pkg_config_libdir = ['{}']

                [host_machine]
                system = 'linux'
                cpu_family = 'arm'
                cpu = 'armv7'
                endian = 'little'
                '''.format(os.path.join(testdir, 'cross_pkgconfig'))))
            crossfile.flush()
            self.meson_cross_files = [crossfile.name]

        env = {'PKG_CONFIG_LIBDIR':  os.path.join(testdir,
                                                  'native_pkgconfig')}
        self.init(testdir, extra_args=['-Dstart_native=false'], override_envvars=env)
        self.wipe()
        self.init(testdir, extra_args=['-Dstart_native=true'], override_envvars=env)

    def __reconfigure(self):
        # Set an older version to force a reconfigure from scratch
        filename = os.path.join(self.privatedir, 'coredata.dat')
        with open(filename, 'rb') as f:
            obj = pickle.load(f)
        obj.version = '0.47.0'
        with open(filename, 'wb') as f:
            pickle.dump(obj, f)

    def test_reconfigure(self):
        testdir = os.path.join(self.unit_test_dir, '47 reconfigure')
        self.init(testdir, extra_args=['-Dopt1=val1', '-Dsub1:werror=true'])
        self.setconf('-Dopt2=val2')

        self.__reconfigure()

        out = self.init(testdir, extra_args=['--reconfigure', '-Dopt3=val3'])
        self.assertRegex(out, 'Regenerating configuration from scratch')
        self.assertRegex(out, 'opt1 val1')
        self.assertRegex(out, 'opt2 val2')
        self.assertRegex(out, 'opt3 val3')
        self.assertRegex(out, 'opt4 default4')
        self.assertRegex(out, 'sub1:werror true')
        self.build()
        self.run_tests()

        # Create a file in builddir and verify wipe command removes it
        filename = os.path.join(self.builddir, 'something')
        open(filename, 'w', encoding='utf-8').close()
        self.assertTrue(os.path.exists(filename))
        out = self.init(testdir, extra_args=['--wipe', '-Dopt4=val4'])
        self.assertFalse(os.path.exists(filename))
        self.assertRegex(out, 'opt1 val1')
        self.assertRegex(out, 'opt2 val2')
        self.assertRegex(out, 'opt3 val3')
        self.assertRegex(out, 'opt4 val4')
        self.assertRegex(out, 'sub1:werror true')
        self.assertTrue(Path(self.builddir, '.gitignore').exists())
        self.build()
        self.run_tests()

    def test_wipe_from_builddir(self):
        testdir = os.path.join(self.common_test_dir, '157 custom target subdir depend files')
        self.init(testdir)
        self.__reconfigure()
        self.init(testdir, extra_args=['--wipe'], workdir=self.builddir)

    def test_target_construct_id_from_path(self):
        # This id is stable but not guessable.
        # The test is supposed to prevent unintentional
        # changes of target ID generation.
        target_id = Target.construct_id_from_path('some/obscure/subdir',
                                                  'target-id', '@suffix')
        self.assertEqual('5e002d3@@target-id@suffix', target_id)
        target_id = Target.construct_id_from_path('subproject/foo/subdir/bar',
                                                  'target2-id', '@other')
        self.assertEqual('81d46d1@@target2-id@other', target_id)

    def test_introspect_projectinfo_without_configured_build(self):
        testfile = os.path.join(self.common_test_dir, '33 run program', 'meson.build')
        res = self.introspect_directory(testfile, '--projectinfo')
        self.assertEqual(set(res['buildsystem_files']), {'meson.build'})
        self.assertEqual(res['version'], 'undefined')
        self.assertEqual(res['descriptive_name'], 'run command')
        self.assertEqual(res['subprojects'], [])

        testfile = os.path.join(self.common_test_dir, '40 options', 'meson.build')
        res = self.introspect_directory(testfile, '--projectinfo')
        self.assertEqual(set(res['buildsystem_files']), {'meson_options.txt', 'meson.build'})
        self.assertEqual(res['version'], 'undefined')
        self.assertEqual(res['descriptive_name'], 'options')
        self.assertEqual(res['subprojects'], [])

        testfile = os.path.join(self.common_test_dir, '43 subproject options', 'meson.build')
        res = self.introspect_directory(testfile, '--projectinfo')
        self.assertEqual(set(res['buildsystem_files']), {'meson_options.txt', 'meson.build'})
        self.assertEqual(res['version'], 'undefined')
        self.assertEqual(res['descriptive_name'], 'suboptions')
        self.assertEqual(len(res['subprojects']), 1)
        subproject_files = {f.replace('\\', '/') for f in res['subprojects'][0]['buildsystem_files']}
        self.assertEqual(subproject_files, {'subprojects/subproject/meson_options.txt', 'subprojects/subproject/meson.build'})
        self.assertEqual(res['subprojects'][0]['name'], 'subproject')
        self.assertEqual(res['subprojects'][0]['version'], 'undefined')
        self.assertEqual(res['subprojects'][0]['descriptive_name'], 'subproject')

    def test_introspect_projectinfo_subprojects(self):
        testdir = os.path.join(self.common_test_dir, '98 subproject subdir')
        self.init(testdir)
        res = self.introspect('--projectinfo')
        expected = {
            'descriptive_name': 'proj',
            'version': 'undefined',
            'subproject_dir': 'subprojects',
            'subprojects': [
                {
                    'descriptive_name': 'sub',
                    'name': 'sub',
                    'version': '1.0'
                },
                {
                    'descriptive_name': 'sub_implicit',
                    'name': 'sub_implicit',
                    'version': '1.0',
                },
                {
                    'descriptive_name': 'sub-novar',
                    'name': 'sub_novar',
                    'version': '1.0',
                },
                {
                    'descriptive_name': 'sub_static',
                    'name': 'sub_static',
                    'version': 'undefined'
                },
                {
                    'descriptive_name': 'subsub',
                    'name': 'subsub',
                    'version': 'undefined'
                },
                {
                    'descriptive_name': 'subsubsub',
                    'name': 'subsubsub',
                    'version': 'undefined'
                },
            ]
        }
        res['subprojects'] = sorted(res['subprojects'], key=lambda i: i['name'])
        self.assertDictEqual(expected, res)

    def test_introspection_target_subproject(self):
        testdir = os.path.join(self.common_test_dir, '42 subproject')
        self.init(testdir)
        res = self.introspect('--targets')

        expected = {
            'sublib': 'sublib',
            'simpletest': 'sublib',
            'user': None
        }

        for entry in res:
            name = entry['name']
            self.assertEqual(entry['subproject'], expected[name])

    def test_introspect_projectinfo_subproject_dir(self):
        testdir = os.path.join(self.common_test_dir, '75 custom subproject dir')
        self.init(testdir)
        res = self.introspect('--projectinfo')

        self.assertEqual(res['subproject_dir'], 'custom_subproject_dir')

    def test_introspect_projectinfo_subproject_dir_from_source(self):
        testfile = os.path.join(self.common_test_dir, '75 custom subproject dir', 'meson.build')
        res = self.introspect_directory(testfile, '--projectinfo')

        self.assertEqual(res['subproject_dir'], 'custom_subproject_dir')

    @skipIfNoExecutable('clang-format')
    def test_clang_format(self):
        if self.backend is not Backend.ninja:
            raise SkipTest(f'Clang-format is for now only supported on Ninja, not {self.backend.name}')
        testdir = os.path.join(self.unit_test_dir, '53 clang-format')

        # Ensure that test project is in git even when running meson from tarball.
        srcdir = os.path.join(self.builddir, 'src')
        shutil.copytree(testdir, srcdir)
        git_init(srcdir)
        testdir = srcdir
        self.new_builddir()

        testfile = os.path.join(testdir, 'prog.c')
        badfile = os.path.join(testdir, 'prog_orig_c')
        goodfile = os.path.join(testdir, 'prog_expected_c')
        testheader = os.path.join(testdir, 'header.h')
        badheader = os.path.join(testdir, 'header_orig_h')
        goodheader = os.path.join(testdir, 'header_expected_h')
        includefile = os.path.join(testdir, '.clang-format-include')
        try:
            shutil.copyfile(badfile, testfile)
            shutil.copyfile(badheader, testheader)
            self.init(testdir)
            self.assertNotEqual(Path(testfile).read_text(encoding='utf-8'),
                                Path(goodfile).read_text(encoding='utf-8'))
            self.assertNotEqual(Path(testheader).read_text(encoding='utf-8'),
                                Path(goodheader).read_text(encoding='utf-8'))

            # test files are not in git so this should do nothing
            self.run_target('clang-format')
            self.assertNotEqual(Path(testfile).read_text(encoding='utf-8'),
                                Path(goodfile).read_text(encoding='utf-8'))
            self.assertNotEqual(Path(testheader).read_text(encoding='utf-8'),
                                Path(goodheader).read_text(encoding='utf-8'))

            # Add an include file to reformat everything
            with open(includefile, 'w', encoding='utf-8') as f:
                f.write('*')
            self.run_target('clang-format')
            self.assertEqual(Path(testheader).read_text(encoding='utf-8'),
                             Path(goodheader).read_text(encoding='utf-8'))
        finally:
            if os.path.exists(testfile):
                os.unlink(testfile)
            if os.path.exists(testheader):
                os.unlink(testheader)
            if os.path.exists(includefile):
                os.unlink(includefile)

    @skipIfNoExecutable('clang-tidy')
    def test_clang_tidy(self):
        if self.backend is not Backend.ninja:
            raise SkipTest(f'Clang-tidy is for now only supported on Ninja, not {self.backend.name}')
        if shutil.which('c++') is None:
            raise SkipTest('Clang-tidy breaks when ccache is used and "c++" not in path.')
        if is_osx():
            raise SkipTest('Apple ships a broken clang-tidy that chokes on -pipe.')
        testdir = os.path.join(self.unit_test_dir, '68 clang-tidy')
        dummydir = os.path.join(testdir, 'dummydir.h')
        self.init(testdir, override_envvars={'CXX': 'c++'})
        out = self.run_target('clang-tidy')
        self.assertIn('cttest.cpp:4:20', out)
        self.assertNotIn(dummydir, out)

    @skipIfNoExecutable('clang-tidy')
    def test_clang_tidy_fix(self):
        if self.backend is not Backend.ninja:
            raise SkipTest(f'Clang-tidy is for now only supported on Ninja, not {self.backend.name}')
        if shutil.which('c++') is None:
            raise SkipTest('Clang-tidy breaks when ccache is used and "c++" not in path.')
        if is_osx():
            raise SkipTest('Apple ships a broken clang-tidy that chokes on -pipe.')
        testdir = os.path.join(self.unit_test_dir, '68 clang-tidy')

        # Ensure that test project is in git even when running meson from tarball.
        srcdir = os.path.join(self.builddir, 'src')
        shutil.copytree(testdir, srcdir)
        git_init(srcdir)
        testdir = srcdir
        self.new_builddir()

        dummydir = os.path.join(testdir, 'dummydir.h')
        testfile = os.path.join(testdir, 'cttest.cpp')
        fixedfile = os.path.join(testdir, 'cttest_fixed.cpp')
        self.init(testdir, override_envvars={'CXX': 'c++'})
        # Make sure test files are different
        self.assertNotEqual(Path(testfile).read_text(encoding='utf-8'),
                            Path(fixedfile).read_text(encoding='utf-8'))
        out = self.run_target('clang-tidy-fix')
        self.assertIn('cttest.cpp:4:20', out)
        self.assertNotIn(dummydir, out)
        # Make sure the test file is fixed
        self.assertEqual(Path(testfile).read_text(encoding='utf-8'),
                         Path(fixedfile).read_text(encoding='utf-8'))

    def test_identity_cross(self):
        testdir = os.path.join(self.unit_test_dir, '69 cross')
        # Do a build to generate a cross file where the host is this target
        self.init(testdir, extra_args=['-Dgenerate=true'])
        self.meson_cross_files = [os.path.join(self.builddir, "crossfile")]
        self.assertTrue(os.path.exists(self.meson_cross_files[0]))
        # Now verify that this is detected as cross
        self.new_builddir()
        self.init(testdir)

    def test_introspect_buildoptions_without_configured_build(self):
        testdir = os.path.join(self.unit_test_dir, '58 introspect buildoptions')
        testfile = os.path.join(testdir, 'meson.build')
        res_nb = self.introspect_directory(testfile, ['--buildoptions'] + self.meson_args)
        self.init(testdir, default_args=False)
        res_wb = self.introspect('--buildoptions')
        self.maxDiff = None
        # XXX: These now generate in a different order, is that okay?
        self.assertListEqual(sorted(res_nb, key=lambda x: x['name']), sorted(res_wb, key=lambda x: x['name']))

    def test_meson_configure_from_source_does_not_crash(self):
        testdir = os.path.join(self.unit_test_dir, '58 introspect buildoptions')
        self._run(self.mconf_command + [testdir])

    def test_introspect_buildoptions_cross_only(self):
        testdir = os.path.join(self.unit_test_dir, '82 cross only introspect')
        testfile = os.path.join(testdir, 'meson.build')
        res = self.introspect_directory(testfile, ['--buildoptions'] + self.meson_args)
        optnames = [o['name'] for o in res]
        self.assertIn('c_args', optnames)
        self.assertNotIn('build.c_args', optnames)

    def test_introspect_json_flat(self):
        testdir = os.path.join(self.unit_test_dir, '56 introspection')
        self.init(testdir, extra_args=['-Dlayout=flat'])
        infodir = os.path.join(self.builddir, 'meson-info')
        self.assertPathExists(infodir)

        with open(os.path.join(infodir, 'intro-targets.json'), encoding='utf-8') as fp:
            targets = json.load(fp)

        for i in targets:
            for out in i['filename']:
                assert os.path.relpath(out, self.builddir).startswith('meson-out')

    def test_introspect_json_dump(self):
        testdir = os.path.join(self.unit_test_dir, '56 introspection')
        self.init(testdir)
        infodir = os.path.join(self.builddir, 'meson-info')
        self.assertPathExists(infodir)

        def assertKeyTypes(key_type_list, obj, strict: bool = True):
            for i in key_type_list:
                if isinstance(i[1], (list, tuple)) and None in i[1]:
                    i = (i[0], tuple(x for x in i[1] if x is not None))
                    if i[0] not in obj or obj[i[0]] is None:
                        continue
                self.assertIn(i[0], obj)
                self.assertIsInstance(obj[i[0]], i[1])
            if strict:
                for k in obj.keys():
                    found = False
                    for i in key_type_list:
                        if k == i[0]:
                            found = True
                            break
                    self.assertTrue(found, f'Key "{k}" not in expected list')

        root_keylist = [
            ('benchmarks', list),
            ('buildoptions', list),
            ('buildsystem_files', list),
            ('compilers', dict),
            ('dependencies', list),
            ('install_plan', dict),
            ('installed', dict),
            ('machines', dict),
            ('projectinfo', dict),
            ('targets', list),
            ('tests', list),
        ]

        test_keylist = [
            ('cmd', list),
            ('env', dict),
            ('name', str),
            ('timeout', int),
            ('suite', list),
            ('is_parallel', bool),
            ('protocol', str),
            ('depends', list),
            ('workdir', (str, None)),
            ('priority', int),
            ('extra_paths', list),
        ]

        buildoptions_keylist = [
            ('name', str),
            ('section', str),
            ('type', str),
            ('description', str),
            ('machine', str),
            ('choices', (list, None)),
            ('value', (str, int, bool, list)),
        ]

        buildoptions_typelist = [
            ('combo', str, [('choices', list)]),
            ('string', str, []),
            ('boolean', bool, []),
            ('integer', int, []),
            ('array', list, []),
        ]

        buildoptions_sections = ['core', 'backend', 'base', 'compiler', 'directory', 'user', 'test']
        buildoptions_machines = ['any', 'build', 'host']

        dependencies_typelist = [
            ('name', str),
            ('type', str),
            ('version', str),
            ('compile_args', list),
            ('link_args', list),
            ('include_directories', list),
            ('sources', list),
            ('extra_files', list),
            ('dependencies', list),
            ('depends', list),
            ('meson_variables', list),
        ]

        targets_typelist = [
            ('name', str),
            ('id', str),
            ('type', str),
            ('defined_in', str),
            ('filename', list),
            ('build_by_default', bool),
            ('target_sources', list),
            ('extra_files', list),
            ('subproject', (str, None)),
            ('dependencies', list),
            ('depends', list),
            ('install_filename', (list, None)),
            ('installed', bool),
            ('vs_module_defs', (str, None)),
            ('win_subsystem', (str, None)),
        ]

        targets_sources_typelist = [
            ('language', str),
            ('compiler', list),
            ('parameters', list),
            ('sources', list),
            ('generated_sources', list),
            ('unity_sources', (list, None)),
        ]

        target_sources_linker_typelist = [
            ('linker', list),
            ('parameters', list),
        ]

        # First load all files
        res = {}
        for i in root_keylist:
            curr = os.path.join(infodir, 'intro-{}.json'.format(i[0]))
            self.assertPathExists(curr)
            with open(curr, encoding='utf-8') as fp:
                res[i[0]] = json.load(fp)

        assertKeyTypes(root_keylist, res)

        # Match target ids to input and output files for ease of reference
        src_to_id = {}
        out_to_id = {}
        name_to_out = {}
        for i in res['targets']:
            print(json.dump(i, sys.stdout))
            out_to_id.update({os.path.relpath(out, self.builddir): i['id']
                              for out in i['filename']})
            name_to_out.update({i['name']: i['filename']})
            for group in i['target_sources']:
                src_to_id.update({os.path.relpath(src, testdir): i['id']
                                  for src in group.get('sources', [])})

        # Check Tests and benchmarks
        tests_to_find = ['test case 1', 'test case 2', 'benchmark 1']
        deps_to_find = {'test case 1': [src_to_id['t1.cpp']],
                        'test case 2': [src_to_id['t2.cpp'], src_to_id['t3.cpp']],
                        'benchmark 1': [out_to_id['file2'], out_to_id['file3'], out_to_id['file4'], src_to_id['t3.cpp']]}
        for i in res['benchmarks'] + res['tests']:
            assertKeyTypes(test_keylist, i)
            if i['name'] in tests_to_find:
                tests_to_find.remove(i['name'])
            self.assertEqual(sorted(i['depends']),
                             sorted(deps_to_find[i['name']]))
        self.assertListEqual(tests_to_find, [])

        # Check buildoptions
        buildopts_to_find = {'cpp_std': 'c++11'}
        for i in res['buildoptions']:
            assertKeyTypes(buildoptions_keylist, i)
            valid_type = False
            for j in buildoptions_typelist:
                if i['type'] == j[0]:
                    self.assertIsInstance(i['value'], j[1])
                    assertKeyTypes(j[2], i, strict=False)
                    valid_type = True
                    break

            self.assertIn(i['section'], buildoptions_sections)
            self.assertIn(i['machine'], buildoptions_machines)
            self.assertTrue(valid_type)
            if i['name'] in buildopts_to_find:
                self.assertEqual(i['value'], buildopts_to_find[i['name']])
                buildopts_to_find.pop(i['name'], None)
        self.assertDictEqual(buildopts_to_find, {})

        # Check buildsystem_files
        bs_files = ['meson.build', 'meson_options.txt', 'sharedlib/meson.build', 'staticlib/meson.build']
        bs_files = [os.path.join(testdir, x) for x in bs_files]
        self.assertPathListEqual(list(sorted(res['buildsystem_files'])), list(sorted(bs_files)))

        # Check dependencies
        dependencies_to_find = ['threads']
        for i in res['dependencies']:
            assertKeyTypes(dependencies_typelist, i)
            if i['name'] in dependencies_to_find:
                dependencies_to_find.remove(i['name'])
        self.assertListEqual(dependencies_to_find, [])

        # Check projectinfo
        self.assertDictEqual(res['projectinfo'], {'version': '1.2.3', 'descriptive_name': 'introspection', 'subproject_dir': 'subprojects', 'subprojects': []})

        # Check targets
        targets_to_find = {
            'sharedTestLib': ('shared library', True, False, 'sharedlib/meson.build',
                              [os.path.join(testdir, 'sharedlib', 'shared.cpp')]),
            'staticTestLib': ('static library', True, False, 'staticlib/meson.build',
                              [os.path.join(testdir, 'staticlib', 'static.c')]),
            'custom target test 1': ('custom', False, False, 'meson.build',
                                     [os.path.join(testdir, 'cp.py')]),
            'custom target test 2': ('custom', False, False, 'meson.build',
                                     name_to_out['custom target test 1']),
            'test1': ('executable', True, True, 'meson.build',
                      [os.path.join(testdir, 't1.cpp')]),
            'test2': ('executable', True, False, 'meson.build',
                      [os.path.join(testdir, 't2.cpp')]),
            'test3': ('executable', True, False, 'meson.build',
                      [os.path.join(testdir, 't3.cpp')]),
            'custom target test 3': ('custom', False, False, 'meson.build',
                                     name_to_out['test3']),
        }
        for i in res['targets']:
            assertKeyTypes(targets_typelist, i)
            if i['name'] in targets_to_find:
                tgt = targets_to_find[i['name']]
                self.assertEqual(i['type'], tgt[0])
                self.assertEqual(i['build_by_default'], tgt[1])
                self.assertEqual(i['installed'], tgt[2])
                self.assertPathEqual(i['defined_in'], os.path.join(testdir, tgt[3]))
                targets_to_find.pop(i['name'], None)
            for j in i['target_sources']:
                if 'compiler' in j:
                    assertKeyTypes(targets_sources_typelist, j)
                    self.assertEqual(j['sources'], [os.path.normpath(f) for f in tgt[4]])
                else:
                    assertKeyTypes(target_sources_linker_typelist, j)
        self.assertDictEqual(targets_to_find, {})

    def test_introspect_file_dump_equals_all(self):
        testdir = os.path.join(self.unit_test_dir, '56 introspection')
        self.init(testdir)
        res_all = self.introspect('--all')
        res_file = {}

        root_keylist = [
            'benchmarks',
            'buildoptions',
            'buildsystem_files',
            'compilers',
            'dependencies',
            'installed',
            'install_plan',
            'machines',
            'projectinfo',
            'targets',
            'tests',
        ]

        infodir = os.path.join(self.builddir, 'meson-info')
        self.assertPathExists(infodir)
        for i in root_keylist:
            curr = os.path.join(infodir, f'intro-{i}.json')
            self.assertPathExists(curr)
            with open(curr, encoding='utf-8') as fp:
                res_file[i] = json.load(fp)

        self.assertEqual(res_all, res_file)

    def test_introspect_meson_info(self):
        testdir = os.path.join(self.unit_test_dir, '56 introspection')
        introfile = os.path.join(self.builddir, 'meson-info', 'meson-info.json')
        self.init(testdir)
        self.assertPathExists(introfile)
        with open(introfile, encoding='utf-8') as fp:
            res1 = json.load(fp)

        for i in ['meson_version', 'directories', 'introspection', 'build_files_updated', 'error']:
            self.assertIn(i, res1)

        self.assertEqual(res1['error'], False)
        self.assertEqual(res1['build_files_updated'], True)

    def test_introspect_config_update(self):
        testdir = os.path.join(self.unit_test_dir, '56 introspection')
        introfile = os.path.join(self.builddir, 'meson-info', 'intro-buildoptions.json')
        self.init(testdir)
        self.assertPathExists(introfile)
        with open(introfile, encoding='utf-8') as fp:
            res1 = json.load(fp)

        for i in res1:
            if i['name'] == 'cpp_std':
                i['value'] = 'c++14'
            if i['name'] == 'build.cpp_std':
                i['value'] = 'c++14'
            if i['name'] == 'buildtype':
                i['value'] = 'release'
            if i['name'] == 'optimization':
                i['value'] = '3'
            if i['name'] == 'debug':
                i['value'] = False

        self.setconf('-Dcpp_std=c++14')
        self.setconf('-Dbuildtype=release')

        with open(introfile, encoding='utf-8') as fp:
            res2 = json.load(fp)

        self.assertListEqual(res1, res2)

    def test_introspect_targets_from_source(self):
        testdir = os.path.join(self.unit_test_dir, '56 introspection')
        testfile = os.path.join(testdir, 'meson.build')
        introfile = os.path.join(self.builddir, 'meson-info', 'intro-targets.json')
        self.init(testdir)
        self.assertPathExists(introfile)
        with open(introfile, encoding='utf-8') as fp:
            res_wb = json.load(fp)

        res_nb = self.introspect_directory(testfile, ['--targets'] + self.meson_args)

        # Account for differences in output
        res_wb = [i for i in res_wb if i['type'] != 'custom']
        for i in res_wb:
            i['filename'] = [os.path.relpath(x, self.builddir) for x in i['filename']]
            for k in ('install_filename', 'dependencies', 'win_subsystem'):
                if k in i:
                    del i[k]

            sources = []
            for j in i['target_sources']:
                sources += j.get('sources', [])
            i['target_sources'] = [{
                'language': 'unknown',
                'compiler': [],
                'parameters': [],
                'sources': sources,
                'generated_sources': []
            }]

        self.maxDiff = None
        self.assertListEqual(res_nb, res_wb)

    def test_introspect_ast_source(self):
        testdir = os.path.join(self.unit_test_dir, '56 introspection')
        testfile = os.path.join(testdir, 'meson.build')
        res_nb = self.introspect_directory(testfile, ['--ast'] + self.meson_args)

        node_counter = {}

        def accept_node(json_node):
            self.assertIsInstance(json_node, dict)
            for i in ['lineno', 'colno', 'end_lineno', 'end_colno']:
                self.assertIn(i, json_node)
                self.assertIsInstance(json_node[i], int)
            self.assertIn('node', json_node)
            n = json_node['node']
            self.assertIsInstance(n, str)
            self.assertIn(n, nodes)
            if n not in node_counter:
                node_counter[n] = 0
            node_counter[n] = node_counter[n] + 1
            for nodeDesc in nodes[n]:
                key = nodeDesc[0]
                func = nodeDesc[1]
                self.assertIn(key, json_node)
                if func is None:
                    tp = nodeDesc[2]
                    self.assertIsInstance(json_node[key], tp)
                    continue
                func(json_node[key])

        def accept_node_list(node_list):
            self.assertIsInstance(node_list, list)
            for i in node_list:
                accept_node(i)

        def accept_kwargs(kwargs):
            self.assertIsInstance(kwargs, list)
            for i in kwargs:
                self.assertIn('key', i)
                self.assertIn('val', i)
                accept_node(i['key'])
                accept_node(i['val'])

        nodes = {
            'BooleanNode': [('value', None, bool)],
            'IdNode': [('value', None, str)],
            'NumberNode': [('value', None, int)],
            'StringNode': [('value', None, str)],
            'FormatStringNode': [('value', None, str)],
            'ContinueNode': [],
            'BreakNode': [],
            'ArgumentNode': [('positional', accept_node_list), ('kwargs', accept_kwargs)],
            'ArrayNode': [('args', accept_node)],
            'DictNode': [('args', accept_node)],
            'EmptyNode': [],
            'OrNode': [('left', accept_node), ('right', accept_node)],
            'AndNode': [('left', accept_node), ('right', accept_node)],
            'ComparisonNode': [('left', accept_node), ('right', accept_node), ('ctype', None, str)],
            'ArithmeticNode': [('left', accept_node), ('right', accept_node), ('op', None, str)],
            'NotNode': [('right', accept_node)],
            'CodeBlockNode': [('lines', accept_node_list)],
            'IndexNode': [('object', accept_node), ('index', accept_node)],
            'MethodNode': [('object', accept_node), ('args', accept_node), ('name', None, str)],
            'FunctionNode': [('args', accept_node), ('name', None, str)],
            'AssignmentNode': [('value', accept_node), ('var_name', None, str)],
            'PlusAssignmentNode': [('value', accept_node), ('var_name', None, str)],
            'ForeachClauseNode': [('items', accept_node), ('block', accept_node), ('varnames', None, list)],
            'IfClauseNode': [('ifs', accept_node_list), ('else', accept_node)],
            'IfNode': [('condition', accept_node), ('block', accept_node)],
            'UMinusNode': [('right', accept_node)],
            'TernaryNode': [('condition', accept_node), ('true', accept_node), ('false', accept_node)],
        }

        accept_node(res_nb)

        for n, c in [('ContinueNode', 2), ('BreakNode', 1), ('NotNode', 3)]:
            self.assertIn(n, node_counter)
            self.assertEqual(node_counter[n], c)

    def test_introspect_dependencies_from_source(self):
        testdir = os.path.join(self.unit_test_dir, '56 introspection')
        testfile = os.path.join(testdir, 'meson.build')
        res_nb = self.introspect_directory(testfile, ['--scan-dependencies'] + self.meson_args)
        expected = [
            {
                'name': 'threads',
                'required': True,
                'version': [],
                'has_fallback': False,
                'conditional': False
            },
            {
                'name': 'zlib',
                'required': False,
                'version': [],
                'has_fallback': False,
                'conditional': False
            },
            {
                'name': 'bugDep1',
                'required': True,
                'version': [],
                'has_fallback': False,
                'conditional': False
            },
            {
                'name': 'somethingthatdoesnotexist',
                'required': True,
                'version': ['>=1.2.3'],
                'has_fallback': False,
                'conditional': True
            },
            {
                'name': 'look_i_have_a_fallback',
                'required': True,
                'version': ['>=1.0.0', '<=99.9.9'],
                'has_fallback': True,
                'conditional': True
            }
        ]
        self.maxDiff = None
        self.assertListEqual(res_nb, expected)

    def test_unstable_coredata(self):
        testdir = os.path.join(self.common_test_dir, '1 trivial')
        self.init(testdir)
        # just test that the command does not fail (e.g. because it throws an exception)
        self._run([*self.meson_command, 'unstable-coredata', self.builddir])

    @skip_if_no_cmake
    def test_cmake_prefix_path(self):
        testdir = os.path.join(self.unit_test_dir, '62 cmake_prefix_path')
        self.init(testdir, extra_args=['-Dcmake_prefix_path=' + os.path.join(testdir, 'prefix')])

    @skip_if_no_cmake
    def test_cmake_parser(self):
        testdir = os.path.join(self.unit_test_dir, '63 cmake parser')
        self.init(testdir, extra_args=['-Dcmake_prefix_path=' + os.path.join(testdir, 'prefix')])

    def test_alias_target(self):
        testdir = os.path.join(self.unit_test_dir, '64 alias target')
        self.init(testdir)
        self.build()
        self.assertPathDoesNotExist(os.path.join(self.builddir, 'prog' + exe_suffix))
        self.assertPathDoesNotExist(os.path.join(self.builddir, 'hello.txt'))
        self.run_target('build-all')
        self.assertPathExists(os.path.join(self.builddir, 'prog' + exe_suffix))
        self.assertPathExists(os.path.join(self.builddir, 'hello.txt'))
        out = self.run_target('aliased-run')
        self.assertIn('a run target was here', out)

    def test_configure(self):
        testdir = os.path.join(self.common_test_dir, '2 cpp')
        self.init(testdir)
        self._run(self.mconf_command + [self.builddir])

    def test_summary(self):
        testdir = os.path.join(self.unit_test_dir, '71 summary')
        out = self.init(testdir, extra_args=['-Denabled_opt=enabled', f'-Dpython={sys.executable}'])
        expected = textwrap.dedent(r'''
            Some Subproject 2.0

                string : bar
                integer: 1
                boolean: true

            subsub undefined

                Something: Some value

            My Project 1.0

              Configuration
                Some boolean   : false
                Another boolean: true
                Some string    : Hello World
                A list         : string
                                 1
                                 true
                empty list     :
                enabled_opt    : enabled
                A number       : 1
                yes            : YES
                no             : NO
                comma list     : a, b, c

              Stuff
                missing prog   : NO
                existing prog  : ''' + ExternalProgram('python3', [sys.executable], silent=True).path + '''
                missing dep    : NO
                external dep   : YES 1.2.3
                internal dep   : YES
                disabler       : NO

              Plugins
                long comma list: alpha, alphacolor, apetag, audiofx, audioparsers, auparse,
                                 autodetect, avi

              Subprojects (for host machine)
                sub            : YES
                sub2           : NO Problem encountered: This subproject failed
                subsub         : YES (from sub2)

              User defined options
                backend        : ''' + self.backend_name + '''
                libdir         : lib
                prefix         : /usr
                enabled_opt    : enabled
                python         : ''' + sys.executable + '''
            ''')
        expected_lines = expected.split('\n')[1:]
        out_start = out.find(expected_lines[0])
        out_lines = out[out_start:].split('\n')[:len(expected_lines)]
        for e, o in zip(expected_lines, out_lines):
            if e.startswith('    external dep'):
                self.assertRegex(o, r'^    external dep   : (YES [0-9.]*|NO)$')
            else:
                self.assertEqual(o, e)

    def test_meson_compile(self):
        """Test the meson compile command."""

        def get_exe_name(basename: str) -> str:
            if is_windows():
                return f'{basename}.exe'
            else:
                return basename

        def get_shared_lib_name(basename: str) -> str:
            if mesonbuild.environment.detect_msys2_arch():
                return f'lib{basename}.dll'
            elif is_windows():
                return f'{basename}.dll'
            elif is_cygwin():
                return f'cyg{basename}.dll'
            elif is_osx():
                return f'lib{basename}.dylib'
            else:
                return f'lib{basename}.so'

        def get_static_lib_name(basename: str) -> str:
            return f'lib{basename}.a'

        # Base case (no targets or additional arguments)

        testdir = os.path.join(self.common_test_dir, '1 trivial')
        self.init(testdir)

        self._run([*self.meson_command, 'compile', '-C', self.builddir])
        self.assertPathExists(os.path.join(self.builddir, get_exe_name('trivialprog')))

        # `--clean`

        self._run([*self.meson_command, 'compile', '-C', self.builddir, '--clean'])
        self.assertPathDoesNotExist(os.path.join(self.builddir, get_exe_name('trivialprog')))

        # Target specified in a project with unique names

        testdir = os.path.join(self.common_test_dir, '6 linkshared')
        self.init(testdir, extra_args=['--wipe'])
        # Multiple targets and target type specified
        self._run([*self.meson_command, 'compile', '-C', self.builddir, 'mylib', 'mycpplib:shared_library'])
        # Check that we have a shared lib, but not an executable, i.e. check that target actually worked
        self.assertPathExists(os.path.join(self.builddir, get_shared_lib_name('mylib')))
        self.assertPathDoesNotExist(os.path.join(self.builddir, get_exe_name('prog')))
        self.assertPathExists(os.path.join(self.builddir, get_shared_lib_name('mycpplib')))
        self.assertPathDoesNotExist(os.path.join(self.builddir, get_exe_name('cppprog')))

        # Target specified in a project with non unique names

        testdir = os.path.join(self.common_test_dir, '185 same target name')
        self.init(testdir, extra_args=['--wipe'])
        self._run([*self.meson_command, 'compile', '-C', self.builddir, './foo'])
        self.assertPathExists(os.path.join(self.builddir, get_static_lib_name('foo')))
        self._run([*self.meson_command, 'compile', '-C', self.builddir, 'sub/foo'])
        self.assertPathExists(os.path.join(self.builddir, 'sub', get_static_lib_name('foo')))

        # run_target

        testdir = os.path.join(self.common_test_dir, '51 run target')
        self.init(testdir, extra_args=['--wipe'])
        out = self._run([*self.meson_command, 'compile', '-C', self.builddir, 'py3hi'])
        self.assertIn('I am Python3.', out)

        # `--$BACKEND-args`

        testdir = os.path.join(self.common_test_dir, '1 trivial')
        if self.backend is Backend.ninja:
            self.init(testdir, extra_args=['--wipe'])
            # Dry run - should not create a program
            self._run([*self.meson_command, 'compile', '-C', self.builddir, '--ninja-args=-n'])
            self.assertPathDoesNotExist(os.path.join(self.builddir, get_exe_name('trivialprog')))
        elif self.backend is Backend.vs:
            self.init(testdir, extra_args=['--wipe'])
            self._run([*self.meson_command, 'compile', '-C', self.builddir])
            # Explicitly clean the target through msbuild interface
            self._run([*self.meson_command, 'compile', '-C', self.builddir, '--vs-args=-t:{}:Clean'.format(re.sub(r'[\%\$\@\;\.\(\)\']', '_', get_exe_name('trivialprog')))])
            self.assertPathDoesNotExist(os.path.join(self.builddir, get_exe_name('trivialprog')))

    def test_spurious_reconfigure_built_dep_file(self):
        testdir = os.path.join(self.unit_test_dir, '73 dep files')

        # Regression test: Spurious reconfigure was happening when build
        # directory is inside source directory.
        # See https://gitlab.freedesktop.org/gstreamer/gst-build/-/issues/85.
        srcdir = os.path.join(self.builddir, 'srctree')
        shutil.copytree(testdir, srcdir)
        builddir = os.path.join(srcdir, '_build')
        self.change_builddir(builddir)

        self.init(srcdir)
        self.build()

        # During first configure the file did not exist so no dependency should
        # have been set. A rebuild should not trigger a reconfigure.
        self.clean()
        out = self.build()
        self.assertNotIn('Project configured', out)

        self.init(srcdir, extra_args=['--reconfigure'])

        # During the reconfigure the file did exist, but is inside build
        # directory, so no dependency should have been set. A rebuild should not
        # trigger a reconfigure.
        self.clean()
        out = self.build()
        self.assertNotIn('Project configured', out)

    def _test_junit(self, case: str) -> None:
        try:
            import lxml.etree as et
        except ImportError:
            raise SkipTest('lxml required, but not found.')

        schema = et.XMLSchema(et.parse(str(Path(self.src_root) / 'data' / 'schema.xsd')))

        self.init(case)
        self.run_tests()

        junit = et.parse(str(Path(self.builddir) / 'meson-logs' / 'testlog.junit.xml'))
        try:
            schema.assertValid(junit)
        except et.DocumentInvalid as e:
            self.fail(e.error_log)

    def test_junit_valid_tap(self):
        self._test_junit(os.path.join(self.common_test_dir, '206 tap tests'))

    def test_junit_valid_exitcode(self):
        self._test_junit(os.path.join(self.common_test_dir, '41 test args'))

    def test_junit_valid_gtest(self):
        self._test_junit(os.path.join(self.framework_test_dir, '2 gtest'))

    def test_link_language_linker(self):
        # TODO: there should be some way to query how we're linking things
        # without resorting to reading the ninja.build file
        if self.backend is not Backend.ninja:
            raise SkipTest('This test reads the ninja file')

        testdir = os.path.join(self.common_test_dir, '225 link language')
        self.init(testdir)

        build_ninja = os.path.join(self.builddir, 'build.ninja')
        with open(build_ninja, encoding='utf-8') as f:
            contents = f.read()

        self.assertRegex(contents, r'build main(\.exe)?.*: c_LINKER')
        self.assertRegex(contents, r'build (lib|cyg)?mylib.*: c_LINKER')

    def test_commands_documented(self):
        '''
        Test that all listed meson commands are documented in Commands.md.
        '''

        # The docs directory is not in release tarballs.
        if not os.path.isdir('docs'):
            raise SkipTest('Doc directory does not exist.')
        doc_path = 'docs/markdown/Commands.md'

        md = None
        with open(doc_path, encoding='utf-8') as f:
            md = f.read()
        self.assertIsNotNone(md)

        ## Get command sections

        section_pattern = re.compile(r'^### (.+)$', re.MULTILINE)
        md_command_section_matches = [i for i in section_pattern.finditer(md)]
        md_command_sections = dict()
        for i, s in enumerate(md_command_section_matches):
            section_end = len(md) if i == len(md_command_section_matches) - 1 else md_command_section_matches[i + 1].start()
            md_command_sections[s.group(1)] = (s.start(), section_end)

        ## Validate commands

        md_commands = {k for k,v in md_command_sections.items()}
        help_output = self._run(self.meson_command + ['--help'])
        # Python's argument parser might put the command list to its own line. Or it might not.
        self.assertTrue(help_output.startswith('usage: '))
        lines = help_output.split('\n')
        line1 = lines[0]
        line2 = lines[1]
        if '{' in line1:
            cmndline = line1
        else:
            self.assertIn('{', line2)
            cmndline = line2
        cmndstr = cmndline.split('{')[1]
        self.assertIn('}', cmndstr)
        help_commands = set(cmndstr.split('}')[0].split(','))
        self.assertTrue(len(help_commands) > 0, 'Must detect some command names.')

        self.assertEqual(md_commands | {'help'}, help_commands, f'Doc file: `{doc_path}`')

        ## Validate that each section has proper placeholders

        def get_data_pattern(command):
            return re.compile(
                r'{{ ' + command + r'_usage.inc }}[\r\n]'
                r'.*?'
                r'{{ ' + command + r'_arguments.inc }}[\r\n]',
                flags = re.MULTILINE|re.DOTALL)

        for command in md_commands:
            m = get_data_pattern(command).search(md, pos=md_command_sections[command][0], endpos=md_command_sections[command][1])
            self.assertIsNotNone(m, f'Command `{command}` is missing placeholders for dynamic data. Doc file: `{doc_path}`')

    def _check_coverage_files(self, types=('text', 'xml', 'html')):
        covdir = Path(self.builddir) / 'meson-logs'
        files = []
        if 'text' in types:
            files.append('coverage.txt')
        if 'xml' in types:
            files.append('coverage.xml')
        if 'html' in types:
            files.append('coveragereport/index.html')
        for f in files:
            self.assertTrue((covdir / f).is_file(), msg=f'{f} is not a file')

    def test_coverage(self):
        if mesonbuild.environment.detect_msys2_arch():
            raise SkipTest('Skipped due to problems with coverage on MSYS2')
        gcovr_exe, gcovr_new_rootdir = mesonbuild.environment.detect_gcovr()
        if not gcovr_exe:
            raise SkipTest('gcovr not found, or too old')
        testdir = os.path.join(self.common_test_dir, '1 trivial')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        if cc.get_id() == 'clang':
            if not mesonbuild.environment.detect_llvm_cov():
                raise SkipTest('llvm-cov not found')
        if cc.get_id() == 'msvc':
            raise SkipTest('Test only applies to non-MSVC compilers')
        self.init(testdir, extra_args=['-Db_coverage=true'])
        self.build()
        self.run_tests()
        self.run_target('coverage')
        self._check_coverage_files()

    def test_coverage_complex(self):
        if mesonbuild.environment.detect_msys2_arch():
            raise SkipTest('Skipped due to problems with coverage on MSYS2')
        gcovr_exe, gcovr_new_rootdir = mesonbuild.environment.detect_gcovr()
        if not gcovr_exe:
            raise SkipTest('gcovr not found, or too old')
        testdir = os.path.join(self.common_test_dir, '105 generatorcustom')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        if cc.get_id() == 'clang':
            if not mesonbuild.environment.detect_llvm_cov():
                raise SkipTest('llvm-cov not found')
        if cc.get_id() == 'msvc':
            raise SkipTest('Test only applies to non-MSVC compilers')
        self.init(testdir, extra_args=['-Db_coverage=true'])
        self.build()
        self.run_tests()
        self.run_target('coverage')
        self._check_coverage_files()

    def test_coverage_html(self):
        if mesonbuild.environment.detect_msys2_arch():
            raise SkipTest('Skipped due to problems with coverage on MSYS2')
        gcovr_exe, gcovr_new_rootdir = mesonbuild.environment.detect_gcovr()
        if not gcovr_exe:
            raise SkipTest('gcovr not found, or too old')
        testdir = os.path.join(self.common_test_dir, '1 trivial')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        if cc.get_id() == 'clang':
            if not mesonbuild.environment.detect_llvm_cov():
                raise SkipTest('llvm-cov not found')
        if cc.get_id() == 'msvc':
            raise SkipTest('Test only applies to non-MSVC compilers')
        self.init(testdir, extra_args=['-Db_coverage=true'])
        self.build()
        self.run_tests()
        self.run_target('coverage-html')
        self._check_coverage_files(['html'])

    def test_coverage_text(self):
        if mesonbuild.environment.detect_msys2_arch():
            raise SkipTest('Skipped due to problems with coverage on MSYS2')
        gcovr_exe, gcovr_new_rootdir = mesonbuild.environment.detect_gcovr()
        if not gcovr_exe:
            raise SkipTest('gcovr not found, or too old')
        testdir = os.path.join(self.common_test_dir, '1 trivial')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        if cc.get_id() == 'clang':
            if not mesonbuild.environment.detect_llvm_cov():
                raise SkipTest('llvm-cov not found')
        if cc.get_id() == 'msvc':
            raise SkipTest('Test only applies to non-MSVC compilers')
        self.init(testdir, extra_args=['-Db_coverage=true'])
        self.build()
        self.run_tests()
        self.run_target('coverage-text')
        self._check_coverage_files(['text'])

    def test_coverage_xml(self):
        if mesonbuild.environment.detect_msys2_arch():
            raise SkipTest('Skipped due to problems with coverage on MSYS2')
        gcovr_exe, gcovr_new_rootdir = mesonbuild.environment.detect_gcovr()
        if not gcovr_exe:
            raise SkipTest('gcovr not found, or too old')
        testdir = os.path.join(self.common_test_dir, '1 trivial')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        if cc.get_id() == 'clang':
            if not mesonbuild.environment.detect_llvm_cov():
                raise SkipTest('llvm-cov not found')
        if cc.get_id() == 'msvc':
            raise SkipTest('Test only applies to non-MSVC compilers')
        self.init(testdir, extra_args=['-Db_coverage=true'])
        self.build()
        self.run_tests()
        self.run_target('coverage-xml')
        self._check_coverage_files(['xml'])

    def test_coverage_escaping(self):
        if mesonbuild.environment.detect_msys2_arch():
            raise SkipTest('Skipped due to problems with coverage on MSYS2')
        gcovr_exe, gcovr_new_rootdir = mesonbuild.environment.detect_gcovr()
        if not gcovr_exe:
            raise SkipTest('gcovr not found, or too old')
        testdir = os.path.join(self.common_test_dir, '243 escape++')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        if cc.get_id() == 'clang':
            if not mesonbuild.environment.detect_llvm_cov():
                raise SkipTest('llvm-cov not found')
        if cc.get_id() == 'msvc':
            raise SkipTest('Test only applies to non-MSVC compilers')
        self.init(testdir, extra_args=['-Db_coverage=true'])
        self.build()
        self.run_tests()
        self.run_target('coverage')
        self._check_coverage_files()

    def test_cross_file_constants(self):
        with temp_filename() as crossfile1, temp_filename() as crossfile2:
            with open(crossfile1, 'w', encoding='utf-8') as f:
                f.write(textwrap.dedent(
                    '''
                    [constants]
                    compiler = 'gcc'
                    '''))
            with open(crossfile2, 'w', encoding='utf-8') as f:
                f.write(textwrap.dedent(
                    '''
                    [constants]
                    toolchain = '/toolchain/'
                    common_flags = ['--sysroot=' + toolchain / 'sysroot']

                    [properties]
                    c_args = common_flags + ['-DSOMETHING']
                    cpp_args = c_args + ['-DSOMETHING_ELSE']
                    rel_to_src = '@GLOBAL_SOURCE_ROOT@' / 'tool'
                    rel_to_file = '@DIRNAME@' / 'tool'
                    no_escaping = '@@DIRNAME@@' / 'tool'

                    [binaries]
                    c = toolchain / compiler
                    '''))

            values = mesonbuild.coredata.parse_machine_files([crossfile1, crossfile2], self.builddir)
            self.assertEqual(values['binaries']['c'], '/toolchain/gcc')
            self.assertEqual(values['properties']['c_args'],
                             ['--sysroot=/toolchain/sysroot', '-DSOMETHING'])
            self.assertEqual(values['properties']['cpp_args'],
                             ['--sysroot=/toolchain/sysroot', '-DSOMETHING', '-DSOMETHING_ELSE'])
            self.assertEqual(values['properties']['rel_to_src'], os.path.join(self.builddir, 'tool'))
            self.assertEqual(values['properties']['rel_to_file'], os.path.join(os.path.dirname(crossfile2), 'tool'))
            self.assertEqual(values['properties']['no_escaping'], os.path.join(f'@{os.path.dirname(crossfile2)}@', 'tool'))

    @skipIf(is_windows(), 'Directory cleanup fails for some reason')
    def test_wrap_git(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            srcdir = os.path.join(tmpdir, 'src')
            shutil.copytree(os.path.join(self.unit_test_dir, '80 wrap-git'), srcdir)
            upstream = os.path.join(srcdir, 'subprojects', 'wrap_git_upstream')
            upstream_uri = Path(upstream).as_uri()
            git_init(upstream)
            with open(os.path.join(srcdir, 'subprojects', 'wrap_git.wrap'), 'w', encoding='utf-8') as f:
                f.write(textwrap.dedent('''
                  [wrap-git]
                  url = {}
                  patch_directory = wrap_git_builddef
                  revision = master
                '''.format(upstream_uri)))
            out = self.init(srcdir)
            self.build()
            self.run_tests()

            # Make sure the warning does not occur on the first init.
            out_of_date_warning = 'revision may be out of date'
            self.assertNotIn(out_of_date_warning, out)

            # Change the wrap's revisions, reconfigure, and make sure it does
            # warn on the reconfigure.
            with open(os.path.join(srcdir, 'subprojects', 'wrap_git.wrap'), 'w', encoding='utf-8') as f:
                f.write(textwrap.dedent('''
                  [wrap-git]
                  url = {}
                  patch_directory = wrap_git_builddef
                  revision = not-master
                '''.format(upstream_uri)))
            out = self.init(srcdir, extra_args='--reconfigure')
            self.assertIn(out_of_date_warning, out)

    def test_extract_objects_custom_target_no_warning(self):
        testdir = os.path.join(self.common_test_dir, '22 object extraction')

        out = self.init(testdir)
        self.assertNotRegex(out, "WARNING:.*can't be converted to File object")

    def test_multi_output_custom_target_no_warning(self):
        testdir = os.path.join(self.common_test_dir, '228 custom_target source')

        out = self.init(testdir)
        self.assertNotRegex(out, 'WARNING:.*Using the first one.')
        self.build()
        self.run_tests()

    @skipUnless(is_linux() and (re.search('^i.86$|^x86$|^x64$|^x86_64$|^amd64$', platform.processor()) is not None),
        'Requires ASM compiler for x86 or x86_64 platform currently only available on Linux CI runners')
    def test_nostdlib(self):
        testdir = os.path.join(self.unit_test_dir, '77 nostdlib')
        machinefile = os.path.join(self.builddir, 'machine.txt')
        with open(machinefile, 'w', encoding='utf-8') as f:
            f.write(textwrap.dedent('''
                [properties]
                c_stdlib = 'mylibc'
                '''))

        # Test native C stdlib
        self.meson_native_files = [machinefile]
        self.init(testdir)
        self.build()

        # Test cross C stdlib
        self.new_builddir()
        self.meson_native_files = []
        self.meson_cross_files = [machinefile]
        self.init(testdir)
        self.build()

    def test_meson_version_compare(self):
        testdir = os.path.join(self.unit_test_dir, '81 meson version compare')
        out = self.init(testdir)
        self.assertNotRegex(out, r'WARNING')

    def test_wrap_redirect(self):
        redirect_wrap = os.path.join(self.builddir, 'redirect.wrap')
        real_wrap = os.path.join(self.builddir, 'foo/subprojects/real.wrap')
        os.makedirs(os.path.dirname(real_wrap))

        # Invalid redirect, filename must have .wrap extension
        with open(redirect_wrap, 'w', encoding='utf-8') as f:
            f.write(textwrap.dedent('''
                [wrap-redirect]
                filename = foo/subprojects/real.wrapper
                '''))
        with self.assertRaisesRegex(WrapException, 'wrap-redirect filename must be a .wrap file'):
            PackageDefinition(redirect_wrap)

        # Invalid redirect, filename cannot be in parent directory
        with open(redirect_wrap, 'w', encoding='utf-8') as f:
            f.write(textwrap.dedent('''
                [wrap-redirect]
                filename = ../real.wrap
                '''))
        with self.assertRaisesRegex(WrapException, 'wrap-redirect filename cannot contain ".."'):
            PackageDefinition(redirect_wrap)

        # Invalid redirect, filename must be in foo/subprojects/real.wrap
        with open(redirect_wrap, 'w', encoding='utf-8') as f:
            f.write(textwrap.dedent('''
                [wrap-redirect]
                filename = foo/real.wrap
                '''))
        with self.assertRaisesRegex(WrapException, 'wrap-redirect filename must be in the form foo/subprojects/bar.wrap'):
            PackageDefinition(redirect_wrap)

        # Correct redirect
        with open(redirect_wrap, 'w', encoding='utf-8') as f:
            f.write(textwrap.dedent('''
                [wrap-redirect]
                filename = foo/subprojects/real.wrap
                '''))
        with open(real_wrap, 'w', encoding='utf-8') as f:
            f.write(textwrap.dedent('''
                [wrap-git]
                url = http://invalid
                '''))
        wrap = PackageDefinition(redirect_wrap)
        self.assertEqual(wrap.get('url'), 'http://invalid')

    @skip_if_no_cmake
    def test_nested_cmake_rebuild(self) -> None:
        # This checks a bug where if a non-meson project is used as a third
        # level (or deeper) subproject it doesn't cause a rebuild if the build
        # files for that project are changed
        testdir = os.path.join(self.unit_test_dir, '84 nested subproject regenerate depends')
        cmakefile = Path(testdir) / 'subprojects' / 'sub2' / 'CMakeLists.txt'
        self.init(testdir)
        self.build()
        with cmakefile.open('a', encoding='utf-8'):
            os.utime(str(cmakefile))
        self.assertReconfiguredBuildIsNoop()

    def test_version_file(self):
        srcdir = os.path.join(self.common_test_dir, '2 cpp')
        self.init(srcdir)
        projinfo = self.introspect('--projectinfo')
        self.assertEqual(projinfo['version'], '1.0.0')

    def test_cflags_cppflags(self):
        envs = {'CPPFLAGS': '-DCPPFLAG',
                'CFLAGS': '-DCFLAG',
                'CXXFLAGS': '-DCXXFLAG'}
        srcdir = os.path.join(self.unit_test_dir, '88 multiple envvars')
        self.init(srcdir, override_envvars=envs)
        self.build()

    def test_build_b_options(self) -> None:
        # Currently (0.57) these do nothing, but they've always been allowed
        srcdir = os.path.join(self.common_test_dir, '2 cpp')
        self.init(srcdir, extra_args=['-Dbuild.b_lto=true'])

    def test_install_skip_subprojects(self):
        testdir = os.path.join(self.unit_test_dir, '92 install skip subprojects')
        self.init(testdir)
        self.build()

        main_expected = [
            '',
            'share',
            'include',
            'foo',
            'bin',
            'share/foo',
            'share/foo/foo.dat',
            'include/foo.h',
            'foo/foofile',
            'bin/foo' + exe_suffix,
        ]
        bar_expected = [
            'bar',
            'share/bar',
            'share/bar/bar.dat',
            'include/bar.h',
            'bin/bar' + exe_suffix,
            'bar/barfile'
        ]
        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        if cc.get_argument_syntax() == 'msvc':
            main_expected.append('bin/foo.pdb')
            bar_expected.append('bin/bar.pdb')
        prefix = destdir_join(self.installdir, self.prefix)
        main_expected = [Path(prefix, p) for p in main_expected]
        bar_expected = [Path(prefix, p) for p in bar_expected]
        all_expected = main_expected + bar_expected

        def check_installed_files(extra_args, expected):
            args = ['install', '--destdir', self.installdir] + extra_args
            self._run(self.meson_command + args, workdir=self.builddir)
            all_files = [p for p in Path(self.installdir).rglob('*')]
            self.assertEqual(sorted(expected), sorted(all_files))
            windows_proof_rmtree(self.installdir)

        check_installed_files([], all_expected)
        check_installed_files(['--skip-subprojects'], main_expected)
        check_installed_files(['--skip-subprojects', 'bar'], main_expected)
        check_installed_files(['--skip-subprojects', 'another'], all_expected)

    def test_adding_subproject_to_configure_project(self) -> None:
        srcdir = os.path.join(self.unit_test_dir, '93 new subproject in configured project')
        self.init(srcdir)
        self.build()
        self.setconf('-Duse-sub=true')
        self.build()

    def test_devenv(self):
        testdir = os.path.join(self.unit_test_dir, '90 devenv')
        self.init(testdir)
        self.build()

        cmd = self.meson_command + ['devenv', '-C', self.builddir]
        script = os.path.join(testdir, 'test-devenv.py')
        app = os.path.join(self.builddir, 'app')
        self._run(cmd + python_command + [script])
        self.assertEqual('This is text.', self._run(cmd + [app]).strip())

        cmd = self.meson_command + ['devenv', '-C', self.builddir, '--dump']
        o = self._run(cmd)
        expected = os.pathsep.join(['/prefix', '$TEST_C', '/suffix'])
        self.assertIn(f'TEST_C="{expected}"', o)
        self.assertIn('export TEST_C', o)

        cmd = self.meson_command + ['devenv', '-C', self.builddir, '--dump', '--dump-format', 'sh']
        o = self._run(cmd)
        expected = os.pathsep.join(['/prefix', '$TEST_C', '/suffix'])
        self.assertIn(f'TEST_C="{expected}"', o)
        self.assertNotIn('export', o)

        cmd = self.meson_command + ['devenv', '-C', self.builddir, '--dump', '--dump-format', 'vscode']
        o = self._run(cmd)
        expected = os.pathsep.join(['/prefix', '/suffix'])
        self.assertIn(f'TEST_C="{expected}"', o)
        self.assertNotIn('export', o)

        fname = os.path.join(self.builddir, 'dump.env')
        cmd = self.meson_command + ['devenv', '-C', self.builddir, '--dump', fname]
        o = self._run(cmd)
        self.assertEqual(o, '')
        o = Path(fname).read_text(encoding='utf-8')
        expected = os.pathsep.join(['/prefix', '$TEST_C', '/suffix'])
        self.assertIn(f'TEST_C="{expected}"', o)
        self.assertIn('export TEST_C', o)

    def test_clang_format_check(self):
        if self.backend is not Backend.ninja:
            raise SkipTest(f'Skipping clang-format tests with {self.backend.name} backend')
        if not shutil.which('clang-format'):
            raise SkipTest('clang-format not found')

        testdir = os.path.join(self.unit_test_dir, '94 clangformat')
        newdir = os.path.join(self.builddir, 'testdir')
        shutil.copytree(testdir, newdir)
        self.new_builddir()
        self.init(newdir)

        # Should reformat 1 file but not return error
        output = self.build('clang-format')
        self.assertEqual(1, output.count('File reformatted:'))

        # Reset source tree then try again with clang-format-check, it should
        # return an error code this time.
        windows_proof_rmtree(newdir)
        shutil.copytree(testdir, newdir)
        with self.assertRaises(subprocess.CalledProcessError):
            output = self.build('clang-format-check')
            self.assertEqual(1, output.count('File reformatted:'))

        # The check format should not touch any files. Thus
        # running format again has some work to do.
        output = self.build('clang-format')
        self.assertEqual(1, output.count('File reformatted:'))
        self.build('clang-format-check')

    def test_custom_target_implicit_include(self):
        testdir = os.path.join(self.unit_test_dir, '95 custominc')
        self.init(testdir)
        self.build()
        compdb = self.get_compdb()
        matches = 0
        for c in compdb:
            if 'prog.c' in c['file']:
                self.assertNotIn('easytogrepfor', c['command'])
                matches += 1
        self.assertEqual(matches, 1)
        matches = 0
        for c in compdb:
            if 'prog2.c' in c['file']:
                self.assertIn('easytogrepfor', c['command'])
                matches += 1
        self.assertEqual(matches, 1)

    def test_env_flags_to_linker(self) -> None:
        # Compilers that act as drivers should add their compiler flags to the
        # linker, those that do not shouldn't
        with mock.patch.dict(os.environ, {'CFLAGS': '-DCFLAG', 'LDFLAGS': '-flto'}):
            env = get_fake_env()

            # Get the compiler so we know which compiler class to mock.
            cc =  detect_compiler_for(env, 'c', MachineChoice.HOST, True, '')
            cc_type = type(cc)

            # Test a compiler that acts as a linker
            with mock.patch.object(cc_type, 'INVOKES_LINKER', True):
                cc =  detect_compiler_for(env, 'c', MachineChoice.HOST, True, '')
                link_args = env.coredata.get_external_link_args(cc.for_machine, cc.language)
                self.assertEqual(sorted(link_args), sorted(['-DCFLAG', '-flto']))

            # And one that doesn't
            with mock.patch.object(cc_type, 'INVOKES_LINKER', False):
                cc =  detect_compiler_for(env, 'c', MachineChoice.HOST, True, '')
                link_args = env.coredata.get_external_link_args(cc.for_machine, cc.language)
                self.assertEqual(sorted(link_args), sorted(['-flto']))

    def test_install_tag(self) -> None:
        testdir = os.path.join(self.unit_test_dir, '99 install all targets')
        self.init(testdir)
        self.build()

        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)

        def shared_lib_name(name):
            if cc.get_id() in {'msvc', 'clang-cl'}:
                return f'bin/{name}.dll'
            elif is_windows():
                return f'bin/lib{name}.dll'
            elif is_cygwin():
                return f'bin/cyg{name}.dll'
            elif is_osx():
                return f'lib/lib{name}.dylib'
            return f'lib/lib{name}.so'

        def exe_name(name):
            if is_windows() or is_cygwin():
                return f'{name}.exe'
            return name

        installpath = Path(self.installdir)

        expected_common = {
            installpath,
            Path(installpath, 'usr'),
        }

        expected_devel = expected_common | {
            Path(installpath, 'usr/include'),
            Path(installpath, 'usr/include/bar-devel.h'),
            Path(installpath, 'usr/include/bar2-devel.h'),
            Path(installpath, 'usr/include/foo1-devel.h'),
            Path(installpath, 'usr/include/foo2-devel.h'),
            Path(installpath, 'usr/include/foo3-devel.h'),
            Path(installpath, 'usr/include/out-devel.h'),
            Path(installpath, 'usr/lib'),
            Path(installpath, 'usr/lib/libstatic.a'),
            Path(installpath, 'usr/lib/libboth.a'),
            Path(installpath, 'usr/lib/libboth2.a'),
            Path(installpath, 'usr/include/ct-header1.h'),
            Path(installpath, 'usr/include/ct-header3.h'),
            Path(installpath, 'usr/include/subdir-devel'),
            Path(installpath, 'usr/include/custom_files'),
            Path(installpath, 'usr/include/custom_files/data.txt'),
        }

        if cc.get_id() in {'msvc', 'clang-cl'}:
            expected_devel |= {
                Path(installpath, 'usr/bin'),
                Path(installpath, 'usr/bin/app.pdb'),
                Path(installpath, 'usr/bin/app2.pdb'),
                Path(installpath, 'usr/bin/both.pdb'),
                Path(installpath, 'usr/bin/both2.pdb'),
                Path(installpath, 'usr/bin/bothcustom.pdb'),
                Path(installpath, 'usr/bin/shared.pdb'),
                Path(installpath, 'usr/bin/versioned_shared-1.pdb'),
                Path(installpath, 'usr/lib/both.lib'),
                Path(installpath, 'usr/lib/both2.lib'),
                Path(installpath, 'usr/lib/bothcustom.lib'),
                Path(installpath, 'usr/lib/shared.lib'),
                Path(installpath, 'usr/lib/versioned_shared.lib'),
                Path(installpath, 'usr/otherbin'),
                Path(installpath, 'usr/otherbin/app-otherdir.pdb'),
            }
        elif is_windows() or is_cygwin():
            expected_devel |= {
                Path(installpath, 'usr/lib/libboth.dll.a'),
                Path(installpath, 'usr/lib/libboth2.dll.a'),
                Path(installpath, 'usr/lib/libshared.dll.a'),
                Path(installpath, 'usr/lib/libbothcustom.dll.a'),
                Path(installpath, 'usr/lib/libversioned_shared.dll.a'),
            }
        else:
            expected_devel |= {
                Path(installpath, 'usr/' + shared_lib_name('versioned_shared')),
            }

        expected_runtime = expected_common | {
            Path(installpath, 'usr/bin'),
            Path(installpath, 'usr/bin/' + exe_name('app')),
            Path(installpath, 'usr/otherbin'),
            Path(installpath, 'usr/otherbin/' + exe_name('app-otherdir')),
            Path(installpath, 'usr/bin/' + exe_name('app2')),
            Path(installpath, 'usr/' + shared_lib_name('shared')),
            Path(installpath, 'usr/' + shared_lib_name('both')),
            Path(installpath, 'usr/' + shared_lib_name('both2')),
        }

        if is_windows() or is_cygwin():
            expected_runtime |= {
                Path(installpath, 'usr/' + shared_lib_name('versioned_shared-1')),
            }
        elif is_osx():
            expected_runtime |= {
                Path(installpath, 'usr/' + shared_lib_name('versioned_shared.1')),
            }
        else:
            expected_runtime |= {
                Path(installpath, 'usr/' + shared_lib_name('versioned_shared') + '.1'),
                Path(installpath, 'usr/' + shared_lib_name('versioned_shared') + '.1.2.3'),
            }

        expected_custom = expected_common | {
            Path(installpath, 'usr/share'),
            Path(installpath, 'usr/share/bar-custom.txt'),
            Path(installpath, 'usr/share/foo-custom.h'),
            Path(installpath, 'usr/share/out1-custom.txt'),
            Path(installpath, 'usr/share/out2-custom.txt'),
            Path(installpath, 'usr/share/out3-custom.txt'),
            Path(installpath, 'usr/share/custom_files'),
            Path(installpath, 'usr/share/custom_files/data.txt'),
            Path(installpath, 'usr/share/excludes'),
            Path(installpath, 'usr/share/excludes/installed.txt'),
            Path(installpath, 'usr/lib'),
            Path(installpath, 'usr/lib/libbothcustom.a'),
            Path(installpath, 'usr/' + shared_lib_name('bothcustom')),
        }

        if is_windows() or is_cygwin():
            expected_custom |= {Path(installpath, 'usr/bin')}
        else:
            expected_runtime |= {Path(installpath, 'usr/lib')}

        expected_runtime_custom = expected_runtime | expected_custom

        expected_all = expected_devel | expected_runtime | expected_custom | {
            Path(installpath, 'usr/share/foo-notag.h'),
            Path(installpath, 'usr/share/bar-notag.txt'),
            Path(installpath, 'usr/share/out1-notag.txt'),
            Path(installpath, 'usr/share/out2-notag.txt'),
            Path(installpath, 'usr/share/out3-notag.txt'),
            Path(installpath, 'usr/share/foo2.h'),
            Path(installpath, 'usr/share/out1.txt'),
            Path(installpath, 'usr/share/out2.txt'),
            Path(installpath, 'usr/share/subproject'),
            Path(installpath, 'usr/share/subproject/aaa.txt'),
            Path(installpath, 'usr/share/subproject/bbb.txt'),
        }

        def do_install(tags, expected_files, expected_scripts):
            cmd = self.meson_command + ['install', '--dry-run', '--destdir', self.installdir]
            cmd += ['--tags', tags] if tags else []
            stdout = self._run(cmd, workdir=self.builddir)
            installed = self.read_install_logs()
            self.assertEqual(sorted(expected_files), sorted(installed))
            self.assertEqual(expected_scripts, stdout.count('Running custom install script'))

        do_install('devel', expected_devel, 0)
        do_install('runtime', expected_runtime, 0)
        do_install('custom', expected_custom, 1)
        do_install('runtime,custom', expected_runtime_custom, 1)
        do_install(None, expected_all, 2)


    def test_install_script_dry_run(self):
        testdir = os.path.join(self.common_test_dir, '53 install script')
        self.init(testdir)
        self.build()

        cmd = self.meson_command + ['install', '--dry-run', '--destdir', self.installdir]
        outputs = self._run(cmd, workdir=self.builddir)

        installpath = Path(self.installdir)
        self.assertFalse((installpath / 'usr/diiba/daaba/file.dat').exists())
        self.assertIn("DRYRUN: Writing file file.dat", outputs)


    def test_introspect_install_plan(self):
        testdir = os.path.join(self.unit_test_dir, '99 install all targets')
        introfile = os.path.join(self.builddir, 'meson-info', 'intro-install_plan.json')
        self.init(testdir)
        self.assertPathExists(introfile)
        with open(introfile, encoding='utf-8') as fp:
            res = json.load(fp)

        env = get_fake_env(testdir, self.builddir, self.prefix)

        def output_name(name, type_):
            target = type_(name=name, subdir=None, subproject=None,
                           for_machine=MachineChoice.HOST, sources=[],
                           structured_sources=None,
                           objects=[], environment=env, compilers=env.coredata.compilers[MachineChoice.HOST],
                           build_only_subproject=False, kwargs={})
            target.process_compilers_late()
            return target.filename

        shared_lib_name = lambda name: output_name(name, SharedLibrary)
        static_lib_name = lambda name: output_name(name, StaticLibrary)
        exe_name = lambda name: output_name(name, Executable)

        expected = {
            'targets': {
                f'{self.builddir}/out1-notag.txt': {
                    'destination': '{datadir}/out1-notag.txt',
                    'tag': None,
                    'subproject': None,
                },
                f'{self.builddir}/out2-notag.txt': {
                    'destination': '{datadir}/out2-notag.txt',
                    'tag': None,
                    'subproject': None,
                },
                f'{self.builddir}/libstatic.a': {
                    'destination': '{libdir_static}/libstatic.a',
                    'tag': 'devel',
                    'subproject': None,
                },
                f'{self.builddir}/' + exe_name('app'): {
                    'destination': '{bindir}/' + exe_name('app'),
                    'tag': 'runtime',
                    'subproject': None,
                },
                f'{self.builddir}/' + exe_name('app-otherdir'): {
                    'destination': '{prefix}/otherbin/' + exe_name('app-otherdir'),
                    'tag': 'runtime',
                    'subproject': None,
                },
                f'{self.builddir}/subdir/' + exe_name('app2'): {
                    'destination': '{bindir}/' + exe_name('app2'),
                    'tag': 'runtime',
                    'subproject': None,
                },
                f'{self.builddir}/' + shared_lib_name('shared'): {
                    'destination': '{libdir_shared}/' + shared_lib_name('shared'),
                    'tag': 'runtime',
                    'subproject': None,
                },
                f'{self.builddir}/' + shared_lib_name('both'): {
                    'destination': '{libdir_shared}/' + shared_lib_name('both'),
                    'tag': 'runtime',
                    'subproject': None,
                },
                f'{self.builddir}/' + static_lib_name('both'): {
                    'destination': '{libdir_static}/' + static_lib_name('both'),
                    'tag': 'devel',
                    'subproject': None,
                },
                f'{self.builddir}/' + shared_lib_name('bothcustom'): {
                    'destination': '{libdir_shared}/' + shared_lib_name('bothcustom'),
                    'tag': 'custom',
                    'subproject': None,
                },
                f'{self.builddir}/' + static_lib_name('bothcustom'): {
                    'destination': '{libdir_static}/' + static_lib_name('bothcustom'),
                    'tag': 'custom',
                    'subproject': None,
                },
                f'{self.builddir}/subdir/' + shared_lib_name('both2'): {
                    'destination': '{libdir_shared}/' + shared_lib_name('both2'),
                    'tag': 'runtime',
                    'subproject': None,
                },
                f'{self.builddir}/subdir/' + static_lib_name('both2'): {
                    'destination': '{libdir_static}/' + static_lib_name('both2'),
                    'tag': 'devel',
                    'subproject': None,
                },
                f'{self.builddir}/out1-custom.txt': {
                    'destination': '{datadir}/out1-custom.txt',
                    'tag': 'custom',
                    'subproject': None,
                },
                f'{self.builddir}/out2-custom.txt': {
                    'destination': '{datadir}/out2-custom.txt',
                    'tag': 'custom',
                    'subproject': None,
                },
                f'{self.builddir}/out3-custom.txt': {
                    'destination': '{datadir}/out3-custom.txt',
                    'tag': 'custom',
                    'subproject': None,
                },
                f'{self.builddir}/subdir/out1.txt': {
                    'destination': '{datadir}/out1.txt',
                    'tag': None,
                    'subproject': None,
                },
                f'{self.builddir}/subdir/out2.txt': {
                    'destination': '{datadir}/out2.txt',
                    'tag': None,
                    'subproject': None,
                },
                f'{self.builddir}/out-devel.h': {
                    'destination': '{includedir}/out-devel.h',
                    'tag': 'devel',
                    'subproject': None,
                },
                f'{self.builddir}/out3-notag.txt': {
                    'destination': '{datadir}/out3-notag.txt',
                    'tag': None,
                    'subproject': None,
                },
            },
            'configure': {
                f'{self.builddir}/foo-notag.h': {
                    'destination': '{datadir}/foo-notag.h',
                    'tag': None,
                    'subproject': None,
                },
                f'{self.builddir}/foo2-devel.h': {
                    'destination': '{includedir}/foo2-devel.h',
                    'tag': 'devel',
                    'subproject': None,
                },
                f'{self.builddir}/foo-custom.h': {
                    'destination': '{datadir}/foo-custom.h',
                    'tag': 'custom',
                    'subproject': None,
                },
                f'{self.builddir}/subdir/foo2.h': {
                    'destination': '{datadir}/foo2.h',
                    'tag': None,
                    'subproject': None,
                },
            },
            'data': {
                f'{testdir}/bar-notag.txt': {
                    'destination': '{datadir}/bar-notag.txt',
                    'tag': None,
                    'subproject': None,
                },
                f'{testdir}/bar-devel.h': {
                    'destination': '{includedir}/bar-devel.h',
                    'tag': 'devel',
                    'subproject': None,
                },
                f'{testdir}/bar-custom.txt': {
                    'destination': '{datadir}/bar-custom.txt',
                    'tag': 'custom',
                    'subproject': None,
                },
                f'{testdir}/subdir/bar2-devel.h': {
                    'destination': '{includedir}/bar2-devel.h',
                    'tag': 'devel',
                    'subproject': None,
                },
                f'{testdir}/subprojects/subproject/aaa.txt': {
                    'destination': '{datadir}/subproject/aaa.txt',
                    'tag': None,
                    'subproject': 'subproject',
                },
                f'{testdir}/subprojects/subproject/bbb.txt': {
                    'destination': '{datadir}/subproject/bbb.txt',
                    'tag': 'data',
                    'subproject': 'subproject',
                },
            },
            'headers': {
                f'{testdir}/foo1-devel.h': {
                    'destination': '{includedir}/foo1-devel.h',
                    'tag': 'devel',
                    'subproject': None,
                },
                f'{testdir}/subdir/foo3-devel.h': {
                    'destination': '{includedir}/foo3-devel.h',
                    'tag': 'devel',
                    'subproject': None,
                },
            },
            'install_subdirs': {
                f'{testdir}/custom_files': {
                    'destination': '{datadir}/custom_files',
                    'tag': 'custom',
                    'subproject': None,
                    'exclude_dirs': [],
                    'exclude_files': [],
                },
                f'{testdir}/excludes': {
                    'destination': '{datadir}/excludes',
                    'tag': 'custom',
                    'subproject': None,
                    'exclude_dirs': ['excluded'],
                    'exclude_files': ['excluded.txt'],
                }
            }
        }

        fix_path = lambda path: os.path.sep.join(path.split('/'))
        expected_fixed = {
            data_type: {
                fix_path(source): {
                    key: fix_path(value) if key == 'destination' else value
                    for key, value in attributes.items()
                }
                for source, attributes in files.items()
            }
            for data_type, files in expected.items()
        }

        for data_type, files in expected_fixed.items():
            for file, details in files.items():
                with self.subTest(key='{}.{}'.format(data_type, file)):
                    self.assertEqual(res[data_type][file], details)

    @skip_if_not_language('rust')
    @unittest.skipIf(not shutil.which('clippy-driver'), 'Test requires clippy-driver')
    def test_rust_clippy(self) -> None:
        if self.backend is not Backend.ninja:
            raise unittest.SkipTest('Rust is only supported with ninja currently')
        # When clippy is used, we should get an exception since a variable named
        # "foo" is used, but is on our denylist
        testdir = os.path.join(self.rust_test_dir, '1 basic')
        self.init(testdir, extra_args=['--werror'], override_envvars={'RUSTC': 'clippy-driver'})
        with self.assertRaises(subprocess.CalledProcessError) as cm:
            self.build()
        self.assertTrue('error: use of a blacklisted/placeholder name `foo`' in cm.exception.stdout or
                        'error: use of a disallowed/placeholder name `foo`' in cm.exception.stdout)

    @skip_if_not_language('rust')
    def test_rust_rlib_linkage(self) -> None:
        if self.backend is not Backend.ninja:
            raise unittest.SkipTest('Rust is only supported with ninja currently')
        template = textwrap.dedent('''\
                use std::process::exit;

                pub fn fun() {{
                    exit({});
                }}
            ''')

        testdir = os.path.join(self.unit_test_dir, '102 rlib linkage')
        gen_file = os.path.join(testdir, 'lib.rs')
        with open(gen_file, 'w', encoding='utf-8') as f:
            f.write(template.format(0))
        self.addCleanup(windows_proof_rm, gen_file)

        self.init(testdir)
        self.build()
        self.run_tests()

        with open(gen_file, 'w', encoding='utf-8') as f:
            f.write(template.format(39))

        self.build()
        with self.assertRaises(subprocess.CalledProcessError) as cm:
            self.run_tests()
        self.assertEqual(cm.exception.returncode, 1)
        self.assertIn('exit status 39', cm.exception.stdout)

    @skip_if_not_language('rust')
    def test_bindgen_drops_invalid(self) -> None:
        if self.backend is not Backend.ninja:
            raise unittest.SkipTest('Rust is only supported with ninja currently')
        testdir = os.path.join(self.rust_test_dir, '12 bindgen')
        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)
        # bindgen understands compiler args that clang understands, but not
        # flags by other compilers
        if cc.get_id() == 'gcc':
            bad_arg = '-fdse'
        elif cc.get_id() == 'msvc':
            bad_arg = '/fastfail'
        else:
            raise unittest.SkipTest('Test only supports GCC and MSVC')
        self.init(testdir, extra_args=[f"-Dc_args=['-DCMD_ARG', '{bad_arg}']"])
        intro = self.introspect(['--targets'])
        for i in intro:
            if i['type'] == 'custom' and i['id'].startswith('rustmod-bindgen'):
                args = i['target_sources'][0]['compiler']
                self.assertIn('-DCMD_ARG', args)
                self.assertIn('-DPROJECT_ARG', args)
                self.assertIn('-DGLOBAL_ARG', args)
                self.assertNotIn(bad_arg, args)
                self.assertNotIn('-mtls-dialect=gnu2', args)
                self.assertNotIn('/fp:fast', args)
                return

    def test_custom_target_name(self):
        testdir = os.path.join(self.unit_test_dir, '100 custom target name')
        self.init(testdir)
        out = self.build()
        if self.backend is Backend.ninja:
            self.assertIn('Generating file.txt with a custom command', out)
            self.assertIn('Generating subdir/file.txt with a custom command', out)

    def test_symlinked_subproject(self):
        testdir = os.path.join(self.unit_test_dir, '107 subproject symlink')
        subproject_dir = os.path.join(testdir, 'subprojects')
        subproject = os.path.join(testdir, 'symlinked_subproject')
        symlinked_subproject = os.path.join(testdir, 'subprojects', 'symlinked_subproject')
        if not os.path.exists(subproject_dir):
            os.mkdir(subproject_dir)
        try:
            os.symlink(subproject, symlinked_subproject)
        except OSError:
            raise SkipTest("Symlinks are not available on this machine")
        self.addCleanup(os.remove, symlinked_subproject)

        self.init(testdir)
        self.build()

    def test_configure_same_noop(self):
        testdir = os.path.join(self.unit_test_dir, '109 configure same noop')
        args = [
            '-Dstring=val',
            '-Dboolean=true',
            '-Dcombo=two',
            '-Dinteger=7',
            '-Darray=[\'three\']',
            '-Dfeature=disabled',
            '--buildtype=plain',
            '--prefix=/abc',
        ]
        self.init(testdir, extra_args=args)

        filename = Path(self.privatedir) / 'coredata.dat'

        olddata = filename.read_bytes()
        oldmtime = os.path.getmtime(filename)

        for opt in ('-Dstring=val', '--buildtype=plain', '-Dfeature=disabled', '-Dprefix=/abc'):
            self.setconf([opt])
            newdata = filename.read_bytes()
            newmtime = os.path.getmtime(filename)
            self.assertEqual(oldmtime, newmtime)
            self.assertEqual(olddata, newdata)
            olddata = newdata
            oldmtime = newmtime

        for opt in ('-Dstring=abc', '--buildtype=release', '-Dfeature=enabled', '-Dprefix=/def'):
            self.setconf([opt])
            newdata = filename.read_bytes()
            newmtime = os.path.getmtime(filename)
            self.assertGreater(newmtime, oldmtime)
            self.assertNotEqual(olddata, newdata)
            olddata = newdata
            oldmtime = newmtime

    def test_c_cpp_stds(self):
        testdir = os.path.join(self.unit_test_dir, '115 c cpp stds')
        self.init(testdir)
        # Invalid values should fail whatever compiler we have
        with self.assertRaises(subprocess.CalledProcessError):
            self.setconf('-Dc_std=invalid')
        with self.assertRaises(subprocess.CalledProcessError):
            self.setconf('-Dc_std=c89,invalid')
        with self.assertRaises(subprocess.CalledProcessError):
            self.setconf('-Dc_std=c++11')
        env = get_fake_env()
        cc = detect_c_compiler(env, MachineChoice.HOST)
        if cc.get_id() == 'msvc':
            # default_option should have selected those
            self.assertEqual(self.getconf('c_std'), 'c89')
            self.assertEqual(self.getconf('cpp_std'), 'vc++11')
            # This is deprecated but works for C
            self.setconf('-Dc_std=gnu99')
            self.assertEqual(self.getconf('c_std'), 'c99')
            # C++ however never accepted that fallback
            with self.assertRaises(subprocess.CalledProcessError):
                self.setconf('-Dcpp_std=gnu++11')
            # The first supported std should be selected
            self.setconf('-Dcpp_std=gnu++11,vc++11,c++11')
            self.assertEqual(self.getconf('cpp_std'), 'vc++11')
        elif cc.get_id() == 'gcc':
            # default_option should have selected those
            self.assertEqual(self.getconf('c_std'), 'gnu89')
            self.assertEqual(self.getconf('cpp_std'), 'gnu++98')
            # The first supported std should be selected
            self.setconf('-Dcpp_std=c++11,gnu++11,vc++11')
            self.assertEqual(self.getconf('cpp_std'), 'c++11')
