project('persubproject options', 'c', 'cpp',
  default_options : ['werror=true',
                     'warning_level=3',
                     'cpp_std=c++11'])

assert(get_option('default_library') == 'both', 'Parent default_library should be "both"')
assert(get_option('werror'))
assert(get_option('warning_level') == '3')
assert(get_option('cpp_std') == 'c++11')


# Check it build both by calling a method only both_libraries target implement
lib = library('lib1', 'foo.c')
lib.get_static_lib()

subproject('sub1')

libcpp14_dep = dependency('libcpp14', fallback: 'sub2', default_options : ['default_library=static'])
exe = executable('test1', 'main.cpp', dependencies : libcpp14_dep)
test('mixing-cpp-version', exe)
