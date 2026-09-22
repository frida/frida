project('sub1', 'c', 'cpp',
 default_options : ['warning_level=0'])

assert(get_option('default_library') == 'both', 'Should inherit parent project default_library')
assert(get_option('warning_level') == '0')
assert(get_option('cpp_std') == 'c++11')

# Check it build both by calling a method only both_libraries target implement
lib = library('lib1', 'foo.c')
lib.get_static_lib()
