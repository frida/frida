# https://github.com/mesonbuild/meson/pull/2291
copy = find_program('copyfile.py')
configure_file(input : 'main.h',
               output : 'main.h',
               command : [copy, '@INPUT@', '@OUTPUT@'])
ctfile = custom_target('emptyfile',
                       input : 'emptyfile.c',
                       output : 'emptyfile.c',
                       command : [copy, '@INPUT@', '@OUTPUT@'])
