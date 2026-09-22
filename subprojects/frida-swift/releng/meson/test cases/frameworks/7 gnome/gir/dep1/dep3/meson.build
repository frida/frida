dep3sources = ['dep3.c', 'dep3.h']

dep3lib = shared_library(
  'dep3lib',
  sources : dep3sources,
  dependencies : gobj,
  install : true
)

dep3gir = gnome.generate_gir(
  dep3lib,
  sources : dep3sources,
  nsversion : '1.0',
  namespace : 'MesonDep3',
  symbol_prefix : 'meson',
  identifier_prefix : 'Meson',
  includes : ['GObject-2.0'],
  install : true
)

dep3_dep = declare_dependency(link_with : dep3lib,
  sources : [dep3gir])
