project('FortranThreads')

if not add_languages('fortran', required: false)
  error('MESON_SKIP_TEST: Fortran language not available.')
endif

# want to be sure that CMake can find dependencies where even if the
# project isn't C, the C language is required to find the library.
threads = dependency('threads', method: 'cmake', required: false)
if not threads.found()
  error('MESON_SKIP_TEST: CMake backend not working for Fortran / threads')
endif
