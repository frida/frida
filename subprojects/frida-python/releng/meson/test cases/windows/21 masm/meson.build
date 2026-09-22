project('test-masm', 'c')

if get_option('backend').startswith('vs')
  error('MESON_SKIP_TEST: masm is not supported by vs backend')
endif

cc = meson.get_compiler('c')

# MASM must be found when using MSVC, otherwise it is optional
if not add_languages('masm', required: cc.get_argument_syntax() == 'msvc')
  error('MESON_SKIP_TEST: masm not available')
endif

executable('app', 'hello.masm')
