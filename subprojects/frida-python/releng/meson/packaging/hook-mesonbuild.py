#!hint/python3

"""
PyInstaller hook to make mesonbuild include everything it needs to.
"""

import os
from glob import glob

from PyInstaller.utils.hooks import collect_data_files

datas = []
hiddenimports = []

def get_all_modules_from_dir(dirname):
    '''
    Get all modules required for Meson itself from directories.
    '''
    modname = os.path.basename(dirname)
    modules = [os.path.splitext(os.path.split(x)[1])[0] for x in glob(os.path.join(dirname, '*'))]
    modules = ['mesonbuild.' + modname + '.' + x for x in modules if not x.startswith('_')]
    return modules

datas += collect_data_files('mesonbuild.scripts', include_py_files=True, excludes=['**/__pycache__'])
datas += collect_data_files('mesonbuild.cmake.data')
datas += collect_data_files('mesonbuild.dependencies.data')

# lazy-loaded
hiddenimports += get_all_modules_from_dir('mesonbuild/dependencies')
# imported by meson.build files
hiddenimports += get_all_modules_from_dir('mesonbuild/modules')
# executed when named on CLI
hiddenimports += get_all_modules_from_dir('mesonbuild/scripts')

# Python packagers want to be minimal and only copy the things
# that they can see being used. They are blind to many things.
hiddenimports += [
    # we run distutils as a subprocess via INTROSPECT_COMMAND.
    'distutils.archive_util',
    'distutils.cmd',
    'distutils.config',
    'distutils.core',
    'distutils.debug',
    'distutils.dep_util',
    'distutils.dir_util',
    'distutils.dist',
    'distutils.errors',
    'distutils.extension',
    'distutils.fancy_getopt',
    'distutils.file_util',
    'distutils.spawn',
    'distutils.util',
    'distutils.version',
    'distutils.command.build_ext',
    'distutils.command.build',
    'distutils.command.install',

    # needed for gtk's find_program() scripts
    'filecmp',
]
