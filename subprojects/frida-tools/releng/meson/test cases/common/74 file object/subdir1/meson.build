prog1 = files('prog.c')
lib1 = files('lib.c')

test('subdir0', executable('subdir0', prog0, lib1), should_fail : true)
test('subdir1', executable('subdir1', prog1, lib0), should_fail : true)

test('subdir2', executable('subdir2', prog1, lib1))