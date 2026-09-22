project('Fortran coarray', 'fortran',
  meson_version: '>=0.50')

fc = meson.get_compiler('fortran')

if ['pgi', 'flang'].contains(fc.get_id())
  error('MESON_SKIP_TEST: At least through PGI 19.10 and Flang 7.1 do not support Fortran Coarrays.')
endif

# coarray is required because single-image fallback is an intrinsic feature
coarray = dependency('coarray')

# check coarray, because user might not have all the library stack installed correctly
# for example, conflicting library/compiler versions on PATH
# this has to invoke a run of "sync all" to verify the MPI stack is functioning,
# particularly for dynamic linking
if fc.run('sync all; end', dependencies: coarray, name: 'Coarray link & run').returncode() != 0
  error('MESON_SKIP_TEST: coarray stack (including MPI) did not link correctly so that a simple test could run.')
endif

exe = executable('hello', 'main.f90',
  dependencies : coarray)

test('Coarray hello world', exe, timeout: 10)
