incdirs = include_directories('include')

p1lib = static_library('proj1', 'proj1f1.c',
  include_directories : incdirs
)

indirect_source = files('proj1f2.c')

proj1_dep = declare_dependency(include_directories : incdirs,
  link_with : p1lib,
  sources : ['proj1f3.c', indirect_source])
