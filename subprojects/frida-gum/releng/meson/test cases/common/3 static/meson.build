project('static library test', 'c', default_options : ['default_library=static'])

lib = static_library('mylib', get_option('source'),
  link_args : '-THISMUSTNOBEUSED') # Static linker needs to ignore all link args.
assert(lib.name() == 'mylib')
has_not_changed = false
if is_disabler(lib)
    has_not_changed = true
else
    has_not_changed = true
endif
assert(has_not_changed, 'Static library has changed.')

assert(not is_disabler(lib), 'Static library is a disabler.')

if get_option('default_library') == 'static'
  library('lib2', 'lib3.c', c_static_args : ['-DWORK'], c_shared_args : ['-DBREAK'])
endif
build_target('lib4', 'lib3.c', c_static_args : ['-DWORK'], target_type : 'static_library')
