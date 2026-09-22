project('manygen', 'c')

if meson.is_cross_build()
  # FIXME error out with skip message once cross test runner
  # recognizes it.
  message('Not running this test during cross build.')
else
  subdir('subdir')

  exe = executable('depuser', 'depuser.c',
    generated)

  test('depuser test', exe)
endif
