project('cmakeSubTest', ['c', 'cpp'])

# Fallback to a CMake subproject
sub_dep = dependency('cmModLib++', version: '>=1.2.3')
exe1 = executable('main', ['main.cpp'], dependencies: [sub_dep])
test('test1', exe1)

# Subproject contains both meson.build and CMakeLists.txt. It should default
# to meson but wrap force cmake.
subproject('force_cmake')

testcase expect_error('Wrap method \'notfound\' is not supported, must be one of: meson, cmake, cargo')
  subproject('broken_method')
endtestcase

# With method=meson we can't use cmake.subproject()
cmake = import('cmake')
testcase expect_error('Wrap method is \'meson\' but we are trying to configure it with cmake')
  cmake.subproject('meson_method')
endtestcase

# cmake.subproject() force cmake method even if meson.build exists.
testcase expect_error('Subproject exists but has no CMakeLists.txt file.')
  cmake.subproject('meson_subp')
endtestcase

# Without specifying the method it defaults to meson even if CMakeLists.txt exists.
testcase expect_error('Subproject exists but has no meson.build file.')
  subproject('cmake_subp')
endtestcase
