project('trivial native test', 'c')

sources = 'trivial.c'
cc = meson.get_compiler('c', native: true)

if meson.is_cross_build()
  native_exe = executable('native-trivialprog', sources : sources, native : true)
  test('native exe in cross build', native_exe)
endif
