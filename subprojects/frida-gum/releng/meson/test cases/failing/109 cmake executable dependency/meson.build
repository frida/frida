project('cmake-executable-dependency', 'c')

if not find_program('cmake', required: false).found()
  error('MESON_SKIP_TEST CMake is not installed')
endif

cmake = import('cmake')
cmlib = cmake.subproject('cmlib')
maind = cmlib.dependency('main')
