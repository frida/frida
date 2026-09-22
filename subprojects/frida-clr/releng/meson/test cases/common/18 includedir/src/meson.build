exe = executable('prog', 'prog.c', 'func.c', include_directories : inc)
test('inc test', exe)

exe2 = executable('prog2', 'prog.c', 'func.c', include_directories : [['../include']])
test('inc test 2', exe2)
