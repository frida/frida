project('gmock test', 'cpp')

# Using gmock without gtest is a pain so just
# don't support that then.

gtest = dependency('gtest', main : true, required : false, method : 'system')
if not gtest.found()
  error('MESON_SKIP_TEST: gtest not installed.')
endif
gmock = dependency('gmock', required : false)
if not gmock.found()
  error('MESON_SKIP_TEST: gmock not installed.')
endif

e = executable('gmocktest', 'gmocktest.cc', dependencies : [gtest, gmock])
test('gmock test', e)
