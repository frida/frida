
project('subproj with includedir', 'c')



compile_check = '''
#include "incfile.h"
'''

if not meson.get_compiler('c').compiles(compile_check, name : 'include in subproj',
                                        include_directories: include_directories('include'))
  error('failed')
endif
