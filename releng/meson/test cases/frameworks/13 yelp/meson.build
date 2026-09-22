project('yelp', 'c')

itstool = find_program('itstool', required: false)
if not itstool.found()
  error('MESON_SKIP_TEST itstool not found.')
endif

subdir('help')
