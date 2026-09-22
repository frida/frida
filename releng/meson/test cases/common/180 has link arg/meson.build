project('has link arg', 'c', 'cpp')

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

if cc.get_argument_syntax() == 'msvc'
  is_arg = '/OPT:REF'
  useless = '/DEBUG'
  isnt_arg = '/iambroken'
else
  is_arg = '-Wl,-L/tmp'
  useless = '-Wl,-L/usr'
  isnt_arg = '-Wl,-iambroken'
endif

assert(cc.has_link_argument(is_arg), 'Arg that should have worked does not work.')
assert(cpp.has_link_argument(is_arg), 'Arg that should have worked does not work.')

if cc.get_id() != 'pgi'
assert(not cc.has_link_argument(isnt_arg), 'Arg that should be broken is not.')
assert(not cpp.has_link_argument(isnt_arg), 'Arg that should be broken is not.')

assert(cc.get_supported_link_arguments([is_arg, isnt_arg, useless]) == [is_arg, useless], 'Arg filtering returned different result.')
assert(cpp.get_supported_link_arguments([is_arg, isnt_arg, useless]) == [is_arg, useless], 'Arg filtering returned different result.')

# Have useless at the end to ensure that the search goes from front to back.
l1 = cc.first_supported_link_argument([isnt_arg, is_arg, isnt_arg, useless])
l2 = cc.first_supported_link_argument(isnt_arg, isnt_arg, isnt_arg)

assert(l1.length() == 1, 'First supported returned wrong result.')
assert(l1.get(0) == is_arg, 'First supported returned wrong argument.')
assert(l2.length() == 0, 'First supported did not return empty array.')

l1 = cpp.first_supported_link_argument([isnt_arg, is_arg, isnt_arg, useless])
l2 = cpp.first_supported_link_argument(isnt_arg, isnt_arg, isnt_arg)

assert(l1.length() == 1, 'First supported returned wrong result.')
assert(l1.get(0) == is_arg, 'First supported returned wrong argument.')
assert(l2.length() == 0, 'First supported did not return empty array.')

assert(not cc.has_multi_link_arguments([isnt_arg, is_arg]), 'Arg that should be broken is not.')

assert(not cc.has_link_argument('-Wl,-z,nodelete42'), 'Did not detect wrong -z linker argument')
endif

assert(cc.has_multi_link_arguments(is_arg), 'Arg that should have worked does not work.')
assert(cc.has_multi_link_arguments([useless, is_arg]), 'Arg that should have worked does not work.')
