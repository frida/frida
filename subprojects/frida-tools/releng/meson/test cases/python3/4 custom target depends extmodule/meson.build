project('Python extension module', 'c',
  default_options : ['buildtype=release'])
# Because Windows Python ships only with optimized libs,
# we must build this project the same way.

py3_mod = import('python3')
py3 = py3_mod.find_python()
py3_dep = dependency('python3', required : false)
cc = meson.get_compiler('c')

# Copy to the builddir so that blaster.py can find the built tachyon module
# FIXME: We should automatically detect this case and append the correct paths
# to PYTHONLIBDIR
blaster_py = configure_file(input : 'blaster.py',
                            output : 'blaster.py',
                            configuration : configuration_data())

check_exists = '''
import os, sys
with open(sys.argv[1], 'rb') as f:
  assert(f.read() == b'success')
'''
if py3_dep.found()
  message('Detected Python version: ' + py3_dep.version())
  if py3_dep.version().version_compare('>=3.8') and cc.get_id() == 'msvc' and cc.version().version_compare('<=19.00.24215.1')
    error('MESON_SKIP_TEST: Python modules do not work with Python 3.8 and VS2015 or earlier.')
  endif

  subdir('ext')

  out_txt = custom_target('tachyon flux',
    input : blaster_py,
    output : 'out.txt',
    command : [py3, '@INPUT@', '-o', '@OUTPUT@'],
    depends : pylib,
    build_by_default: true)

  test('flux', py3, args : ['-c', check_exists, out_txt])
else
  error('MESON_SKIP_TEST: Python3 libraries not found, skipping test.')
endif
