project('test spec', 'c')

rpm = import('rpm')
dependency('zlib')
find_program('nonexistprog', required : false)

lib = shared_library('mesontest_shared', ['lib.c', 'lib.h'],
                     version : '0.1', soversion : '0',
                     install : true)
executable('mesontest-bin', 'main.c',
           link_with : lib,
           install : true)

rpm.generate_spec_template()
