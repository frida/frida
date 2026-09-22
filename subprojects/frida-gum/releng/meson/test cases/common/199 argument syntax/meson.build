project(
  'argument syntax',
  ['c'],
)

cc = meson.get_compiler('c')

if ['gcc', 'lcc', 'clang', 'intel'].contains(cc.get_id())
  expected = 'gcc'
elif ['msvc', 'clang-cl', 'intel-cl'].contains(cc.get_id())
  expected = 'msvc'
else
  # It's possible that other compilers end up here that shouldn't
  expected = 'other'
endif

assert(cc.get_argument_syntax() == expected,
       'Wrong output for compiler @0@. expected @1@ but got @2@'.format(
         cc.get_id(), expected, cc.get_argument_syntax()))
