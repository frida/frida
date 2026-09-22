libsources = ['meson-subsample.c', 'meson-subsample.h']

girlib = shared_library(
  'girlib',
  sources : libsources,
  dependencies : [gobj, statichelper_dep],
  install : true
)

girexe = executable(
  'girprog',
  sources : 'prog.c',
  dependencies : [gobj, statichelper_dep],
  link_with : girlib
)

gnome.generate_gir(
  girlib, statichelper_lib,
  sources : [ libsources, statichelper_sources ],
  nsversion : '1.0',
  namespace : 'Meson',
  symbol_prefix : 'meson_',
  identifier_prefix : 'Meson',
  includes : ['GObject-2.0'],
  install : true
)

test('gobject introspection/subproject/c', girexe)
