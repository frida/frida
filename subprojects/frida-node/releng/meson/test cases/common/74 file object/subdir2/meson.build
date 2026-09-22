prog2 = files('prog.c')
lib2 = files('lib.c')

test('subdir3', executable('subdir3', prog1, lib2), should_fail : true)
test('subdir4', executable('subdir4', prog2, lib1), should_fail : true)

test('subdir4', executable('subdir5', prog2, lib2))