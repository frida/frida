# on Ubuntu this test requires libgtest-dev
project('gtest', 'cpp')

gtest = dependency('gtest', main : true, required : false)
if not gtest.found()
  error('MESON_SKIP_TEST: gtest not installed.')
endif
gtest_nomain = dependency('gtest', main : false, method : 'system')

e = executable('testprog', 'test.cc', dependencies : gtest)
test('gtest test', e, protocol : 'gtest')

e = executable('testprog_nomain', 'test_nomain.cc', dependencies : gtest_nomain)
test('gtest nomain test', e, protocol : 'gtest')
