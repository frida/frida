project('cython', 'c',
  default_options : ['warning_level=3'])

cython = find_program('cython3', required : false)
py3_dep = dependency('python3', required : false)

if cython.found() and py3_dep.found()
  py3_dep = dependency('python3')
  py3_dep_majver = py3_dep.version().split('.')
  py3_dep_majver = py3_dep_majver[0] + '.' + py3_dep_majver[1]
  py3_mod = import('python3')
  py3 = py3_mod.find_python()
  if py3_dep_majver != py3_mod.language_version()
      v = py3_mod.language_version()
      error('MESON_SKIP_TEST: deprecated python3 module is non-functional when default python3 is different from Meson\'s', v)
  endif
  subdir('libdir')

  test('cython tester',
    py3,
    args : files('cytest.py'),
    env : ['PYTHONPATH=' + pydir]
  )
else
  error('MESON_SKIP_TEST: Cython3 or Python3 libraries not found, skipping test.')
endif
