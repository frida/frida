project('cmake-executable-dependency')

if not find_program('cmake', required: false).found()
  error('MESON_SKIP_TEST CMake is not installed')
endif

cmake = import('cmake')
cmlib = cmake.subproject('cmlib')
