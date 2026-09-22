project('C, C++ and Fortran', 'c', 'cpp', 'fortran')

cpp = meson.get_compiler('cpp')
fc = meson.get_compiler('fortran')

if build_machine.system() == 'windows' and fc.get_id() == 'gcc' and cpp.get_id() != 'gcc'
  error('MESON_SKIP_TEST mixing gfortran with non-GNU C++ does not work.')
endif

link_with = []
if fc.get_id() == 'intel'
  link_with += fc.find_library('ifport')
endif

e = executable(
  'cfort',
  ['main.c', 'fortran.f'],
  dependencies : link_with,
)

test('C and Fortran', e)

e2 = executable(
  'cppfort',
  ['main.cpp', 'fortran.f'],
  dependencies : link_with,
)

test('C++ and Fortran', e2)
