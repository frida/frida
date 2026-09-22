project('test features', 'c')

tester = executable('tester', 'tester.c')
test_with_status = executable('test-with-status', 'tester_with_status.c')
test('nonzero return code no tests', tester, args : [], protocol: 'tap')
test('nonzero return code with tests', test_with_status, protocol: 'tap')
test('missing test', tester, args : ['1..1'], protocol: 'tap')
test('incorrect skip', tester, args : ['1..1 # skip\nok 1'], protocol: 'tap')
test('partially skipped', tester, args : ['not ok 1\nok 2 # skip'], protocol: 'tap')
