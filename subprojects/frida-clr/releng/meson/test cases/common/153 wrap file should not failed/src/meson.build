executable('grabprog3', files('subprojects/prog.c'))
executable('grabprog4', files('subprojects/foo/prog2.c'))

texe = executable('testexe', files('test.c'), link_with: [libfoo, libbar])

test('t1', texe)
