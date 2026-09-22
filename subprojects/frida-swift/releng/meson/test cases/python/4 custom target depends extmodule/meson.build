project('Python extension module', 'c',
  default_options : ['buildtype=release'])
# Because Windows Python ships only with optimized libs,
# we must build this project the same way.

if meson.backend() != 'ninja'
  error('MESON_SKIP_TEST: Ninja backend required')
endif

py_mod = import('python')
py3 = py_mod.find_installation()
py3_dep = py3.dependency(required : false)
cc = meson.get_compiler('c')

if not py3_dep.found()
  error('MESON_SKIP_TEST: Python3 libraries not found, skipping test.')
endif

# Copy to the builddir so that blaster.py can find the built tachyon module
# FIXME: We should automatically detect this case and append the correct paths
# to PYTHONLIBDIR
blaster_py = configure_file(input : 'blaster.py',
                            output : 'blaster.py',
                            copy : true)

check_exists = '''
import os, sys
with open(sys.argv[1], 'rb') as f:
  assert(f.read() == b'success')
'''

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
