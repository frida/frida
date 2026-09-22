conf = configuration_data()
conf.set('HEADER', 'glib-object.h')

meson_sample_header = configure_file(
  input : 'meson-sample.h.in',
  output : 'meson-sample.h',
  configuration : conf)

libsources = ['meson-sample.c', meson_sample_header]

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
  export_packages : 'meson',
  install : true,
  install_gir: false,
)
meson_gir = girtarget[0]
meson_typelib = girtarget[1]

girlib_inc = include_directories('.')
girlib_dep = declare_dependency(link_with : girlib,
  include_directories : [girlib_inc],
  dependencies : [gobj],
  # Everything that uses libgst needs this built to compile
  sources : girtarget,
)
