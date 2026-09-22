project('multiple test suites', 'c')

subproject('sub')

exe1 = executable('exe1', 'exe1.c')
exe2 = executable('exe2', 'exe2.c')

test('exe1', exe1)
test('exe2', exe2, suite : ['suite2', ['super-special']])
