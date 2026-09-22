project('hdf5_framework', 'c')

# NOTE: all HDF5 languages must have HDF5 C library working.

method = get_option('method')

# --- C tests
h5c = dependency('hdf5', language : 'c', required : false, method : method)
if not h5c.found()
  error('MESON_SKIP_TEST: HDF5 C library not found.')
endif
exec = executable('exec', 'main.c', dependencies : h5c)
test('HDF5 C', exec, timeout: 30)

# --- C++ tests
if add_languages('cpp', required: false)
  h5cpp = dependency('hdf5', language : 'cpp', required : false, disabler: true, method : method)
  execpp = executable('execpp', 'main.cpp', dependencies : h5cpp)
  test('HDF5 C++', execpp, timeout: 30)
endif

test_fortran = add_languages('fortran', required: false)

if test_fortran
  cpp = meson.get_compiler('cpp')
  fc = meson.get_compiler('fortran')

  if host_machine.system() == 'darwin' and cpp.get_id() == 'clang' and fc.get_id() == 'gcc'
    # Search paths don't work correctly here and -lgfortran doesn't work
    test_fortran = false
  elif host_machine.system() == 'windows' and cpp.get_id() != 'gcc' and fc.get_id() == 'gcc'
    # mixing gfortran with non-gcc doesn't work on windows
    test_fortran = false
  endif

  # --- Fortran tests
  if test_fortran
    h5f = dependency('hdf5', language : 'fortran', required : false, disabler: true, method : method)
    exef = executable('exef', 'main.f90', dependencies : h5f)
    test('HDF5 Fortran', exef, timeout: 30)
  endif
endif

# Check we can apply a version constraint
if h5c.version() != 'unknown'
  dependency('hdf5', version: '>=@0@'.format(h5c.version()))
endif
