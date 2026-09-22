project('mpi', 'c', 'cpp', default_options: ['b_asneeded=false'])

method = get_option('method')

cc = meson.get_compiler('c')
mpic = dependency('mpi', language : 'c', required : false, method : method)
if not mpic.found()
  error('MESON_SKIP_TEST: MPI not found, skipping.')
endif
exec = executable('exec',
  'main.c',
  dependencies : [mpic])

test('MPI C', exec, timeout: 20)


# C++ MPI not supported by MS-MPI
cpp = meson.get_compiler('cpp')
mpicpp = dependency('mpi', language : 'cpp', required: false, method : method)
if not cpp.links('''
#include <mpi.h>
#include <stdio.h>
int main(int argc, char **argv) {MPI::Init(argc, argv);}
''', dependencies: mpicpp, name: 'C++ MPI')
  mpicpp = disabler()
endif
execpp = executable('execpp',
  'main.cpp',
  dependencies : [mpicpp])

test('MPI C++', execpp, timeout: 20)


if add_languages('fortran', required : false)
  fc = meson.get_compiler('fortran')
  mpif = dependency('mpi', language : 'fortran', required: false, method : method)
  if not fc.links('use mpi; end', dependencies: mpif, name: 'Fortran MPI')
    mpif = disabler()
  endif

  exef = executable('exef',
  'main.f90',
  dependencies : mpif)

  test('MPI Fortran', exef, timeout: 20)
endif


# Check we can apply a version constraint
if mpic.version() != 'unknown'
  dependency('mpi', version: '>=@0@'.format(mpic.version()), method : method)
endif
