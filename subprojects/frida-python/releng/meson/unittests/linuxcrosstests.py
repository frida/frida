# SPDX-License-Identifier: Apache-2.0
# Copyright 2016-2021 The Meson development team

import os
import shutil
import unittest
import platform

from mesonbuild.mesonlib import (
    is_windows, is_cygwin
)
from mesonbuild.mesonlib import MesonException



from .baseplatformtests import BasePlatformTests
from .helpers import *

class BaseLinuxCrossTests(BasePlatformTests):
    # Don't pass --libdir when cross-compiling. We have tests that
    # check whether meson auto-detects it correctly.
    libdir = None


def should_run_cross_arm_tests():
    return shutil.which('armv7l-unknown-linux-gnueabihf-gcc') and not platform.machine().lower().startswith('arm')

@unittest.skipUnless(not is_windows() and should_run_cross_arm_tests(), "requires ability to cross compile to ARM")
class LinuxCrossArmTests(BaseLinuxCrossTests):
    '''
    Tests that cross-compilation to Linux/ARM works
    '''

    def setUp(self):
        super().setUp()
        self.meson_cross_files = [os.path.join(self.src_root, 'cross', 'nixos-armhf.ini')]

    def test_nested_for_build_subprojects(self) -> None:
        """Test that when cross compiled nested native subprojects report
        themselves as for the build machine.

        This cannot be done without a unit test because in a host == build
        configuration this wont be reported at all
        """
        testdir = os.path.join(self.src_root, 'test cases', 'native', '10 native subproject')

        with self.subTest('configuring'):
            out = self.init(testdir)

        with self.subTest('nested build-only subproject reports correct machine'):
            self.assertIn('Executing subproject buildtool:hostp for machine: build', out)

        with self.subTest('summary reports correct host subprojects'):
            expected = '\n'.join([
                'Subprojects (for host machine)',
                '    both                : YES',
                '    recursive-both      : YES',
                '    recursive-host-only : YES',
            ])
            self.assertIn(expected, out)

        with self.subTest('summary reports correct build subprojects'):
            expected = '\n'.join([
                'Subprojects (for build machine)',
                '    both                : YES',
                '    buildtool           : YES',
                '    hostp               : YES',
                '    recursive-both      : YES',
                '    recursive-build-only: YES',
                '    test installs       : YES',
            ])
            self.assertIn(expected, out)

    def test_cflags_cross_environment_pollution(self):
        '''
        Test that the CFLAGS environment variable does not pollute the cross
        environment. This can't be an ordinary test case because we need to
        inspect the compiler database.
        '''
        testdir = os.path.join(self.common_test_dir, '3 static')
        self.init(testdir, override_envvars={'CFLAGS': '-DBUILD_ENVIRONMENT_ONLY'})
        compdb = self.get_compdb()
        self.assertNotIn('-DBUILD_ENVIRONMENT_ONLY', compdb[0]['command'])

    def test_cross_file_overrides_always_args(self):
        '''
        Test that $lang_args in cross files always override get_always_args().
        Needed for overriding the default -D_FILE_OFFSET_BITS=64 on some
        architectures such as some Android versions and Raspbian.
        https://github.com/mesonbuild/meson/issues/3049
        https://github.com/mesonbuild/meson/issues/3089
        '''
        testdir = os.path.join(self.unit_test_dir, '33 cross file overrides always args')
        self.meson_cross_files = [os.path.join(testdir, 'ubuntu-armhf-overrides.txt')]
        self.init(testdir)
        compdb = self.get_compdb()
        self.assertRegex(compdb[0]['command'], '-D_FILE_OFFSET_BITS=64.*-U_FILE_OFFSET_BITS')
        self.build()

    def test_cross_libdir(self):
        # When cross compiling "libdir" should default to "lib"
        # rather than "lib/x86_64-linux-gnu" or something like that.
        testdir = os.path.join(self.common_test_dir, '1 trivial')
        self.init(testdir)
        for i in self.introspect('--buildoptions'):
            if i['name'] == 'libdir':
                self.assertEqual(i['value'], 'lib')
                return
        self.assertTrue(False, 'Option libdir not in introspect data.')

    def test_cross_libdir_subproject(self):
        # Guard against a regression where calling "subproject"
        # would reset the value of libdir to its default value.
        testdir = os.path.join(self.unit_test_dir, '75 subdir libdir')
        self.init(testdir, extra_args=['--libdir=fuf'])
        for i in self.introspect('--buildoptions'):
            if i['name'] == 'libdir':
                self.assertEqual(i['value'], 'fuf')
                return
        self.assertTrue(False, 'Libdir specified on command line gets reset.')

    def test_std_remains(self):
        # C_std defined in project options must be in effect also when cross compiling.
        testdir = os.path.join(self.unit_test_dir, '50 noncross options')
        self.init(testdir)
        compdb = self.get_compdb()
        self.assertRegex(compdb[0]['command'], '-std=c99')
        self.build()

    @skipIfNoPkgconfig
    def test_pkg_config_option(self):
        if not shutil.which('arm-linux-gnueabihf-pkg-config'):
            raise unittest.SkipTest('Cross-pkgconfig not found.')
        testdir = os.path.join(self.unit_test_dir, '57 pkg_config_path option')
        self.init(testdir, extra_args=[
            '-Dbuild.pkg_config_path=' + os.path.join(testdir, 'build_extra_path'),
            '-Dpkg_config_path=' + os.path.join(testdir, 'host_extra_path'),
        ])

    def test_run_native_test(self):
        '''
        https://github.com/mesonbuild/meson/issues/7997
        check run native test in crossbuild without exe wrapper
        '''
        testdir = os.path.join(self.unit_test_dir, '87 run native test')
        stamp_file = os.path.join(self.builddir, 'native_test_has_run.stamp')
        self.init(testdir)
        self.build()
        self.assertPathDoesNotExist(stamp_file)
        self.run_tests()
        self.assertPathExists(stamp_file)


def should_run_cross_mingw_tests():
    return shutil.which('x86_64-w64-mingw32-gcc') and not (is_windows() or is_cygwin())

@unittest.skipUnless(not is_windows() and should_run_cross_mingw_tests(), "requires ability to cross compile with MinGW")
class LinuxCrossMingwTests(BaseLinuxCrossTests):
    '''
    Tests that cross-compilation to Windows/MinGW works
    '''

    def setUp(self):
        super().setUp()
        self.meson_cross_files = [os.path.join(self.src_root, 'cross', 'linux-mingw-w64-64bit.txt')]

    def test_exe_wrapper_behaviour(self):
        '''
        Test that an exe wrapper that isn't found doesn't cause compiler sanity
        checks and compiler checks to fail, but causes configure to fail if it
        requires running a cross-built executable (custom_target or run_target)
        and causes the tests to be skipped if they are run.
        '''
        testdir = os.path.join(self.unit_test_dir, '36 exe_wrapper behaviour')
        # Configures, builds, and tests fine by default
        self.init(testdir)
        self.build()
        self.run_tests()
        self.wipe()
        os.mkdir(self.builddir)
        # Change cross file to use a non-existing exe_wrapper and it should fail
        self.meson_cross_files = [os.path.join(testdir, 'broken-cross.txt')]
        # Force tracebacks so we can detect them properly
        env = {'MESON_FORCE_BACKTRACE': '1'}
        error_message = "An exe_wrapper is needed but was not found. Please define one in cross file and check the command and/or add it to PATH."

        with self.assertRaises(MesonException) as cm:
            # Must run in-process or we'll get a generic CalledProcessError
            self.init(testdir, extra_args='-Drun-target=false',
                      inprocess=True,
                      override_envvars=env)
        self.assertEqual(str(cm.exception), error_message)

        with self.assertRaises(MesonException) as cm:
            # Must run in-process or we'll get a generic CalledProcessError
            self.init(testdir, extra_args='-Dcustom-target=false',
                      inprocess=True,
                      override_envvars=env)
        self.assertEqual(str(cm.exception), error_message)

        self.init(testdir, extra_args=['-Dcustom-target=false', '-Drun-target=false'],
                  override_envvars=env)
        self.build()

        with self.assertRaises(MesonException) as cm:
            # Must run in-process or we'll get a generic CalledProcessError
            self.run_tests(inprocess=True, override_envvars=env)
        self.assertEqual(str(cm.exception),
                         "The exe_wrapper defined in the cross file 'broken' was not found. Please check the command and/or add it to PATH.")

    @skipIfNoPkgconfig
    def test_cross_pkg_config_option(self):
        testdir = os.path.join(self.unit_test_dir, '57 pkg_config_path option')
        self.init(testdir, extra_args=[
            '-Dbuild.pkg_config_path=' + os.path.join(testdir, 'build_extra_path'),
            '-Dpkg_config_path=' + os.path.join(testdir, 'host_extra_path'),
        ])
