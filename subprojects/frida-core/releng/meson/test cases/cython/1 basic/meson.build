project(
  'basic cython project',
  ['cython', 'c'],
  default_options : ['warning_level=3', 'buildtype=release'],
)

if meson.backend() != 'ninja'
  error('MESON_SKIP_TEST: Ninja backend required')
endif

py3 = import('python').find_installation()
py3_dep = py3.dependency(required : false)
if not py3_dep.found()
  error('MESON_SKIP_TEST: Python library not found.')
endif

subdir('libdir')

test('cython tester',
  py3,
  args : files('cytest.py'),
  env : ['PYTHONPATH=' + pydir]
)
