project('myexe', 'c')

lib1 = static_library('lib1', 'lib1.c')
lib2 = static_library('lib2', 'lib2.c')

executable('myexe',
           'myexe.c',
           link_with: [lib1, lib2])
