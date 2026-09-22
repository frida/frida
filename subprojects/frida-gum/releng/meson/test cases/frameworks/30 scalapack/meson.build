project('test scalapack', 'c')

mpi_c = dependency('mpi', language: 'c', required: false)
if not mpi_c.found()
  error('MESON_SKIP_TEST: MPI library not available')
endif

scalapack = dependency('scalapack', required: false)
if not scalapack.found()
  error('MESON_SKIP_TEST: scalapack library not available')
endif

c_exe = executable('scalapack_c', 'main.c',
  dependencies: [scalapack, mpi_c])
test('scalapack_c', c_exe, timeout: 30)

if add_languages('fortran', required: false)
  mpi_f = dependency('mpi', language: 'fortran', required: false)
  if not mpi_f.found()
    error('MESON_SKIP_TEST: MPI Fortran library not available')
  endif

  f_exe = executable('scalapack_fortran', 'main.f90',
    dependencies: [scalapack, mpi_f])
  test('scalapack_fortran', f_exe, timeout: 30)
endif
