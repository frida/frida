project('has arg', 'c', 'cpp')

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

if cc.get_id() == 'msvc'
  is_arg = '/O2'
  useless = '/DFOO'
else
  is_arg = '-O2'
  useless = '-DFOO'
endif

isnt_arg = '-fiambroken'

assert(cc.has_argument(is_arg), 'Arg that should have worked does not work.')
assert(not cc.has_argument(isnt_arg), 'Arg that should be broken is not.')

assert(cpp.has_argument(is_arg), 'Arg that should have worked does not work.')
assert(not cpp.has_argument(isnt_arg), 'Arg that should be broken is not.')

assert(cc.get_supported_arguments([is_arg, isnt_arg, useless]) == [is_arg, useless], 'Arg filtering returned different result.')
assert(cpp.get_supported_arguments([is_arg, isnt_arg, useless]) == [is_arg, useless], 'Arg filtering returned different result.')

# Have useless at the end to ensure that the search goes from front to back.
l1 = cc.first_supported_argument([isnt_arg, is_arg, isnt_arg, useless])
l2 = cc.first_supported_argument(isnt_arg, isnt_arg, isnt_arg)

assert(l1.length() == 1, 'First supported returned wrong result.')
assert(l1.get(0) == is_arg, 'First supported returned wrong argument.')
assert(l2.length() == 0, 'First supported did not return empty array.')

l1 = cpp.first_supported_argument([isnt_arg, is_arg, isnt_arg, useless])
l2 = cpp.first_supported_argument(isnt_arg, isnt_arg, isnt_arg)

assert(l1.length() == 1, 'First supported returned wrong result.')
assert(l1.get(0) == is_arg, 'First supported returned wrong argument.')
assert(l2.length() == 0, 'First supported did not return empty array.')

if cc.get_id() == 'gcc'
  pre_arg = '-Wformat'
  # NOTE: We have special handling for -Wno-foo args because gcc silently
  # ignores unknown -Wno-foo args unless you pass -Werror, so for this test, we
  # pass it as two separate arguments.
  anti_pre_arg = ['-W', 'no-format']
  arg = '-Werror=format-security'
  assert(not cc.has_multi_arguments([anti_pre_arg, arg]), 'Arg that should be broken is not.')
  assert(cc.has_multi_arguments(pre_arg), 'Arg that should have worked does not work.')
  assert(cc.has_multi_arguments([pre_arg, arg]), 'Arg that should have worked does not work.')
  # Test that gcc correctly errors out on unknown -Wno flags
  assert(not cc.has_argument('-Wno-lol-meson-test-flags'), 'should error out on unknown -Wno args')
  assert(not cc.has_multi_arguments(['-Wno-pragmas', '-Wno-lol-meson-test-flags']), 'should error out even if some -Wno args are valid')
endif

if cc.get_id() == 'clang' and cc.version().version_compare('<=4.0.0')
  # 4.0.0 does not support -fpeel-loops. Newer versions may.
  # Please adjust above version number as new versions of clang are released.
  notyet_arg = '-fpeel-loops'
  assert(not cc.has_argument(notyet_arg), 'Arg that should be broken (unless clang added support recently) is not.')
endif
