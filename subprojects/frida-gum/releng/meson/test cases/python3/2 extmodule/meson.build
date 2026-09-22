project('Python extension module', 'c',
  default_options : ['buildtype=release'])
# Because Windows Python ships only with optimized libs,
# we must build this project the same way.

py3_mod = import('python3')
py3 = py3_mod.find_python()
py3_dep = dependency('python3', required : false)
cc = meson.get_compiler('c')

if py3_dep.found()
  message('Detected Python version: ' + py3_dep.version())
  # Building extensions for Python 3 using Visual Studio 2015
  # no longer works (or, rather, they build but don't run).
  # Disable the tests in this case.
  if py3_dep.version().version_compare('>=3.8') and cc.get_id() == 'msvc' and cc.version().version_compare('<=19.00.24215.1')
    error('MESON_SKIP_TEST: Python modules do not work with Python 3.8 and VS2015 or earlier.')
  endif

  subdir('ext')

  test('extmod',
    py3,
    args : files('blaster.py'),
    env : ['PYTHONPATH=' + pypathdir])

  # Check we can apply a version constraint
  dependency('python3', version: '>=@0@'.format(py3_dep.version()))

else
  error('MESON_SKIP_TEST: Python3 libraries not found, skipping test.')
endif

py3_pkg_dep = dependency('python3', method: 'pkg-config', required : false)
if py3_pkg_dep.found()
  python_lib_dir = py3_pkg_dep.get_pkgconfig_variable('libdir')
endif
