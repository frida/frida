e = executable('someexe', 'main.c', ctfile,
               c_args : ['-I' + sub3],
               include_directories : j,
               dependencies : dep)

correctinc = declare_dependency(include_directories : include_directories('.'))
