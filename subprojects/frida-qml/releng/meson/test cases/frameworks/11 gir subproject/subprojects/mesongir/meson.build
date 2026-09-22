project('gobject-introspection-subproject', 'c')

gnome = import('gnome')
gobj = dependency('gobject-2.0')

libsources = ['meson-sample.c', 'meson-sample.h']

girlib = shared_library(
  'girlib',
  sources : libsources,
  dependencies : gobj,
  install : true
)

girtarget = gnome.generate_gir(
  girlib,
  sources : libsources,
  nsversion : '1.0',
  namespace : 'Meson',
  symbol_prefix : 'meson_',
  identifier_prefix : 'Meson',
  includes : ['GObject-2.0'],
  install : true
)

meson_gir = declare_dependency(link_with : girlib,
  include_directories : [include_directories('.')],
  dependencies : [gobj],
  # Everything that uses libgst needs this built to compile
  sources : girtarget,
)
