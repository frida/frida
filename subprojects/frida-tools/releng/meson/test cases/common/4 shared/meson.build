project('shared library test', 'c', default_options : ['default_library=shared'])
lib = shared_library('mylib', 'libfile.c')
build_target('mylib2', 'libfile.c', target_type: 'shared_library')

has_not_changed = false
if is_disabler(lib)
    has_not_changed = true
else
    has_not_changed = true
endif
assert(has_not_changed, 'Shared library has changed.')

assert(not is_disabler(lib), 'Shared library is a disabler.')

if get_option('default_library') == 'shared'
  library('mylib5', 'libfile2.c', c_shared_args : ['-DWORK'])
endif
build_target('mylib4', 'libfile2.c', target_type: 'shared_library', c_shared_args : ['-DWORK'], c_static_args : ['-DBREAK'])
