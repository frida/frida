project('subprjfail', 'c')

test('subprjfail-failing_test',
  executable('failing_test', 'failing_test.c'),
  suite : 'fail')

test('subprjfail-failing_test_no_suite',
  executable('failing_test_no_suite', 'failing_test.c'),
  suite : [])
