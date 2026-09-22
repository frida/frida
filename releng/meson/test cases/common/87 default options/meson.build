project('default options', 'cpp', 'c', default_options : [
  'prefix=/absoluteprefix',
  'buildtype=debugoptimized',
  'cpp_std=c++11',
  'cpp_eh=none',
  'warning_level=3',
  'sub1:test_option=false',
  ])

assert(get_option('buildtype') == 'debugoptimized', 'Build type default value wrong.')

cpp_eh = get_option('cpp_eh')
assert(cpp_eh == 'none', 'EH value is "' + cpp_eh + '" instead of "none"')
cpp_std = get_option('cpp_std')
assert(cpp_std == 'c++11', 'C++ std value is "' + cpp_std + '" instead of c++11.')

w_level = get_option('warning_level')
assert(w_level == '3', 'warning level "' + w_level + '" instead of "3"')

# FIXME. Since we no longer accept invalid options to c_std etc,
# there is no simple way to test this. Gcc does not seem to expose
# the C std used in a preprocessor token so we can't check for it.
# Think of a way to fix this.
#
# # Verify that project args are not used when told not to.
# # MSVC plain C does not have a simple arg to test so skip it.
# if cpp.get_id() != 'msvc'
#   cc = meson.get_compiler('c')
#   assert(not cc.compiles('int foobar;'), 'Default arg not used in test.')
#   assert(cc.compiles('int foobar;', no_builtin_args : true), 'No_builtin did not disable builtins.')
# endif

subproject('sub1')
