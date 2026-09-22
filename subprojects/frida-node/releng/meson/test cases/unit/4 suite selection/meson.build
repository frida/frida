project('mainprj', 'c')

subproject('subprjfail')
subproject('subprjsucc')
subproject('subprjmix')

test('mainprj-failing_test',
  executable('failing_test', 'failing_test.c'),
  suite : 'fail')

test('mainprj-successful_test',
  executable('successful_test', 'successful_test.c'),
  suite : 'success')

test('mainprj-successful_test_no_suite',
  executable('no_suite_test', 'successful_test.c'),
  suite : [])
