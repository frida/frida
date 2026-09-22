# This test is complementary to and extends "common/190 openmp" so that
# we can examine more compilers and options than would be warranted in
# the common test where C/C++ must also be handled.
project('openmp', 'fortran',
  meson_version: '>= 0.46')


fc = meson.get_compiler('fortran')
if fc.get_id() == 'gcc' and fc.version().version_compare('<4.2.0')
  error('MESON_SKIP_TEST gcc is too old to support OpenMP.')
endif
if host_machine.system() == 'darwin'
  error('MESON_SKIP_TEST macOS does not support OpenMP.')
endif

openmp = dependency('openmp')

env = environment()
env.set('OMP_NUM_THREADS', '2')

exef = executable('exef',
  'main.f90',
  dependencies : [openmp])
test('OpenMP Fortran', exef, env : env)

openmp_f = dependency('openmp', language : 'fortran')
exe_f = executable('exe_f',
  'main.f90',
  dependencies : [openmp_f])
test('OpenMP Fortran-specific', exe_f, env : env)


# Check we can apply a version constraint
dependency('openmp', version: '>=@0@'.format(openmp.version()))
