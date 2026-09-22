project('threads', 'c')

thread_dep = dependency('threads')


lib1 = static_library('lib1', 'lib1.c',
                      dependencies : thread_dep)

lib2 = static_library('lib2', 'lib2.c',
                       link_with : lib1)

executable('prog', 'prog.c',
           link_with : lib2)
