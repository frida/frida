project('python sample', 'c')

py3_mod = import('python3')
py3 = py3_mod.find_python()

py3_version = py3_mod.language_version()
if py3_version.version_compare('< 3.2')
  error('Invalid python version!?')
endif

py3_purelib = py3_mod.sysconfig_path('purelib')
message('Python purelib:', py3_purelib)
if not (py3_purelib.endswith('site-packages') or py3_purelib.endswith('dist-packages'))
  error('Python3 purelib path seems invalid?')
endif

# could be 'lib64' or 'Lib' on some systems
py3_platlib = py3_mod.sysconfig_path('platlib')
message('Python platlib:', py3_platlib)
if not (py3_platlib.endswith('site-packages') or py3_platlib.endswith('dist-packages'))
  error('Python3 platlib path seems invalid?')
endif

# could be 'Include' on Windows
py3_include = py3_mod.sysconfig_path('include')
if not py3_include.to_lower().startswith('include')
  error('Python3 include path seems invalid?')
endif

main = files('prog.py')

test('toplevel', py3, args : main)

subdir('subdir')
