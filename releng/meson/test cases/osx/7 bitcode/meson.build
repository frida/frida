project('bitcode test', 'c', 'objc', 'objcpp',
    default_options : ['b_bitcode=true'])

both_libraries('alib', 'libfoo.m')
shared_module('amodule', 'libfoo.m')

both_libraries('blib', 'libbar.mm')
shared_module('bmodule', 'libbar.mm')

both_libraries('clib', 'libfile.c')
shared_module('cmodule', 'libfile.c')
