project('charset', 'c')

cc = meson.get_compiler('c')

if cc.get_id() != 'msvc'
  error('MESON_SKIP_TEST requires MSVC.')
endif

executable('utf8', 'utf8.c')

if get_option('test-failure')
  executable('iso-8859-1', 'iso-8859-1.c')
else
  executable('iso-8859-1', 'iso-8859-1.c', c_args: '/source-charset:.850')
endif
