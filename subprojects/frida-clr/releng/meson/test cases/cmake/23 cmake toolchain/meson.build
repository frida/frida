project('cmake toolchain test', ['c'])

if meson.is_cross_build()
  error('MESON_SKIP_TEST: skip this on cross builds')
endif

cm = import('cmake')

sub_pro = cm.subproject('cmMod')

add_languages('cpp')

sub_pro = cm.subproject('cmModFortran')
