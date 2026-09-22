project('native file install dir override', 'cpp')

if meson.is_cross_build()
  error('MESON_SKIP_TEST cannot test native build rules in cross build')
endif

executable('main', 'main.cpp', install : true)
