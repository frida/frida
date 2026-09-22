subdir('dep2')
subdir('dep3')

dep1sources = ['dep1.c', 'dep1.h']

# Do not need to link to dep2lib because we don't use any symbols from it
dep1lib = shared_library(
  'dep1lib',
  sources : dep1sources,
  dependencies : gobj,
  install : true
)

# But the gir does need it because it we use the MesonDep2* structure defined
# in the header
dep1gir = gnome.generate_gir(
  dep1lib,
  sources : dep1sources,
  nsversion : '1.0',
  namespace : 'MesonDep1',
  symbol_prefix : 'meson',
  identifier_prefix : 'Meson',
  header: 'dep1.h',
  includes : ['GObject-2.0', 'MesonDep2-1.0', dep3gir[0]],
  dependencies : [dep2_dep],
  install : true
)

dep1_dep = declare_dependency(link_with : dep1lib,
  dependencies : [dep2_dep, dep3_dep],
  sources : [dep1gir])
