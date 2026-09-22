libsources = ['meson-subsample.c', 'meson-subsample.h']

girsubproject = shared_library(
  'girsubproject',
  sources : libsources,
  dependencies : [gobj, girlib_dep],
  install : true
)

girexe = executable(
  'girprog',
  sources : 'prog.c',
  dependencies : [gobj, girlib_dep],
  link_with : girsubproject
)

gnome.generate_gir(
  girsubproject,
  sources : libsources,
  nsversion : '1.0',
  namespace : 'MesonSub',
  symbol_prefix : 'meson_sub_',
  identifier_prefix : 'MesonSub',
  includes : ['GObject-2.0', meson_gir],
  install : true,
  install_dir_gir: false,
)

message('TEST: ' + girsubproject.outdir())

test('gobject introspection/subproject/c', girexe)
