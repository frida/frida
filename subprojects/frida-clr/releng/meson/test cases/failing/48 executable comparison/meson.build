project('executable comparison', 'c')

exe1 = executable('prog1', sources : 'prog.c')
exe2 = executable('prog2', sources : 'prog.c')

assert(exe1 < exe2, 'should fail')
