project('openmp', 'c')

cc = meson.get_compiler('c')
if cc.get_id() == 'gcc' and cc.version().version_compare('<4.2.0')
  error('MESON_SKIP_TEST gcc is too old to support OpenMP.')
endif
if cc.get_id() == 'clang' and cc.version().version_compare('<3.7.0')
  error('MESON_SKIP_TEST clang is too old to support OpenMP.')
endif
if cc.get_id() == 'msvc' and cc.version().version_compare('<17')
  error('MESON_SKIP_TEST msvc is too old to support OpenMP.')
endif
if cc.get_id() == 'clang-cl' and cc.version().version_compare('<10.0.0')
  error('MESON_SKIP_TEST clang-cl is too old to support OpenMP.')
endif
if cc.get_id() == 'clang' and host_machine.system() == 'windows'
  error('MESON_SKIP_TEST Windows clang does not support OpenMP.')
endif
if host_machine.system() == 'darwin'
  error('MESON_SKIP_TEST macOS does not support OpenMP.')
endif

openmp = dependency('openmp')
env = environment()
env.set('OMP_NUM_THREADS', '2')

exec = executable('exec',
  'main.c',
  dependencies : [openmp])
test('OpenMP C', exec, env : env)

if not(build_machine.system() == 'windows' and cc.get_id() == 'pgi')
  if add_languages('cpp', required : false)
    execpp = executable('execpp',
      'main.cpp',
      dependencies : [openmp])
    test('OpenMP C++', execpp, env : env)
  endif
endif

if add_languages('fortran', required : false)
  # Mixing compilers (msvc/clang with gfortran) does not seem to work on Windows.
  if build_machine.system() != 'windows' or cc.get_id() == 'gnu'
    exef = executable('exef',
      'main.f90',
      dependencies : [openmp])
    test('OpenMP Fortran', exef, env : env)

    openmp_f = dependency('openmp', language : 'fortran')
    exe_f = executable('exe_f',
      'main.f90',
      dependencies : [openmp_f])
    test('OpenMP Fortran-specific', exe_f, env : env)
  endif
endif

# Check we can apply a version constraint
dependency('openmp', version: '>=@0@'.format(openmp.version()))
