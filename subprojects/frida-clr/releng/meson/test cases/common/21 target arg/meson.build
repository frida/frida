project('local arg test', 'cpp', 'c')

exe1 = executable('prog', 'prog.cc', 'func.c', \
c_args   : '-DCTHING', \
cpp_args : '-DCPPTHING')
exe2 = executable('prog2', 'prog2.cc', 'func2.c')

test('prog1', exe1)
test('prog2', exe2)
