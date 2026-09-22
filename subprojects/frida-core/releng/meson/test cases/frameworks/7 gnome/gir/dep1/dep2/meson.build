dep2sources = ['dep2.c', 'dep2.h']

dep2lib = shared_library(
  'dep2lib',
  sources : dep2sources,
  dependencies : gobj,
  install : true
)

dep2gir = gnome.generate_gir(
  dep2lib,
  sources : dep2sources,
  nsversion : '1.0',
  namespace : 'MesonDep2',
  symbol_prefix : 'meson',
  identifier_prefix : 'Meson',
  includes : ['GObject-2.0'],
  install : true
)

dep2_dep = declare_dependency(link_with : dep2lib,
  sources : [dep2gir])
