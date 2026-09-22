project('generated llvm ir', 'c')

if meson.get_compiler('c').get_id() != 'clang'
  error('MESON_SKIP_TEST: LLVM IR files can only be built with clang')
endif

if meson.backend() == 'xcode'
  error('MESON_SKIP_TEST: LLVM ir not supported with the Xcode backend. Patches welcome.')
endif

copy = find_program('copyfile.py')

copygen = generator(copy,
  arguments : ['@INPUT@', '@OUTPUT@'],
  output : '@BASENAME@')

l = library('square-gen', copygen.process('square.ll.in'))

test('square-gen-test', executable('square-gen-test', 'main.c', link_with : l))

copyct = custom_target('square',
  input : 'square.ll.in',
  output : 'square.ll',
  command : [copy, '@INPUT@', '@OUTPUT@'])

l = library('square-ct', copyct)

test('square-ct-test', executable('square-ct-test', 'main.c', link_with : l))
