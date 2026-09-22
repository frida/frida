project('sub2', 'c', 'cpp',
 default_options : ['default_library=shared',
                    'werror=false',
                    'cpp_std=c++14'])

assert(get_option('default_library') == 'static', 'Parent should override default_library')
assert(not get_option('werror'))
assert(get_option('cpp_std') == 'c++14')

# If it doesn't build only a static library, it would make target name clash.
library('lib1', 'foo.c')
shared_library('lib1', 'foo.c')

# Parent project is c++11 but this one uses c++14 to build.
libcpp14 = library('lib2', 'foo.cpp')
meson.override_dependency('libcpp14', declare_dependency(link_with: libcpp14))

# Compiler checks should be using c++14 as well
cxx = meson.get_compiler('cpp')
assert(cxx.compiles(files('foo.cpp')))
