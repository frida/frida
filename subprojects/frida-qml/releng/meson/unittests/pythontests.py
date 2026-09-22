# SPDX-License-Identifier: Apache-2.0
# Copyright 2016-2021 The Meson development team

import glob, os, pathlib, shutil, subprocess, unittest

from run_tests import (
    Backend
)

from .allplatformstests import git_init
from .baseplatformtests import BasePlatformTests
from .helpers import *

from mesonbuild.mesonlib import MachineChoice, TemporaryDirectoryWinProof
from mesonbuild.modules.python import PythonModule

class PythonTests(BasePlatformTests):
    '''
    Tests that verify compilation of python extension modules
    '''

    def test_bad_versions(self):
        if self.backend is not Backend.ninja:
            raise unittest.SkipTest(f'Skipping python tests with {self.backend.name} backend')

        testdir = os.path.join(self.src_root, 'test cases', 'python', '8 different python versions')

        # The test is configured to error out with MESON_SKIP_TEST
        # in case it could not find python
        with self.assertRaises(unittest.SkipTest):
            self.init(testdir, extra_args=['-Dpython=not-python'])
        self.wipe()

        # While dir is an external command on both Windows and Linux,
        # it certainly isn't python
        with self.assertRaises(unittest.SkipTest):
            self.init(testdir, extra_args=['-Dpython=dir'])
        self.wipe()

    def test_dist(self):
        with TemporaryDirectoryWinProof() as dirstr:
            dirobj = pathlib.Path(dirstr)
            mesonfile = dirobj / 'meson.build'
            mesonfile.write_text('''project('test', 'c', version: '1')
pymod = import('python')
python = pymod.find_installation('python3', required: true)
''', encoding='utf-8')
            git_init(dirstr)
            self.init(dirstr)
            subprocess.check_call(self.meson_command + ['dist', '-C', self.builddir], stdout=subprocess.DEVNULL)

    def _test_bytecompile(self, py2=False):
        testdir = os.path.join(self.src_root, 'test cases', 'python', '2 extmodule')

        env = get_fake_env(testdir, self.builddir, self.prefix)
        cc = detect_c_compiler(env, MachineChoice.HOST)

        self.init(testdir, extra_args=['-Dpython2=auto', '-Dpython.bytecompile=1'])
        self.build()
        self.install()

        count = 0
        for root, dirs, files in os.walk(self.installdir):
            for file in files:
                realfile = os.path.join(root, file)
                if file.endswith('.py'):
                    cached = glob.glob(realfile+'?') + glob.glob(os.path.join(root, '__pycache__', os.path.splitext(file)[0] + '*.pyc'))
                    if py2 and cc.get_id() == 'msvc':
                        # MSVC python installs python2/python3 into the same directory
                        self.assertLength(cached, 4)
                    else:
                        self.assertLength(cached, 2)
                    count += 1
        # there are 5 files x 2 installations
        if py2 and not cc.get_id() == 'msvc':
            self.assertEqual(count, 10)
        else:
            self.assertEqual(count, 5)

    def test_bytecompile_multi(self):
        if not shutil.which('python2') and not PythonModule._get_win_pythonpath('python2'):
            raise self.skipTest('python2 not installed')
        self._test_bytecompile(True)

    def test_bytecompile_single(self):
        if shutil.which('python2') or PythonModule._get_win_pythonpath('python2'):
            raise self.skipTest('python2 installed, already tested')
        self._test_bytecompile()
