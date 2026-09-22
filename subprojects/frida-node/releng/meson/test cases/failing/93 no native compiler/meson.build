project('no native compiler')

if not meson.is_cross_build()
   error('MESON_SKIP_TEST test only applicable when cross building.')
endif

if add_languages('c', required: false, native: true)
  error('MESON_SKIP_TEST test only applicable when native compiler not available.')
endif

add_languages('c')
executable('main', 'main.c', native: true)
