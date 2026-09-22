# We use Python3 as it's the only thing guaranteed to be available on any platform Meson can run on (unlike Zlib in linuxlike/13 cmake dependency).

project('user CMake find_package module using cmake_module_path', ['c', 'cpp'],
  meson_version: '>= 0.55.0')

if not find_program('cmake', required: false).found()
  error('MESON_SKIP_TEST cmake binary not available.')
endif

# NOTE: can't request Python3 via dependency('Python3', method: 'cmake')
#  Meson intercepts and wants "method: auto"

# Try to find a dependency with a custom CMake module

dependency('SomethingLikePython', required : true, method : 'cmake', cmake_module_path : 'cmake', modules: 'Python::Interpreter')

dependency('SomethingLikePython', method : 'cmake', cmake_module_path : ['doesNotExist', 'cmake'], modules: 'Python::Interpreter')

# Test a custom target with Python::Interpreter in COMMAND
cm = import('cmake')
op = cm.subproject_options()
op.add_cmake_defines({'CMAKE_MODULE_PATH': meson.source_root() / 'cmake'})
sp = cm.subproject('cmMod', options: op)
main = sp.target('main')
test('main', main)
