project('test priorities')

test_prog = find_program('testprog.py')

test('priority 0', test_prog,
     args : ['0'],
)

test('priority neg 10', test_prog,
     args : ['-10'],
     priority : -10
)

test('priority 1000', test_prog,
     args : ['1000'],
     priority : 1000
)

test('priority 50', test_prog,
     args : ['50'],
     priority : 50
)
