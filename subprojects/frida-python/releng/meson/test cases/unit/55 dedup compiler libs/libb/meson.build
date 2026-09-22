libb = library('b', 'libb.c',
  dependencies: liba_dep)

libb_dep = declare_dependency(link_with: libb,
  include_directories: include_directories('.'),
  dependencies: liba_dep)
