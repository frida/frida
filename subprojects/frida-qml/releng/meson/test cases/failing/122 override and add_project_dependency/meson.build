project('super', 'c')

inc = include_directories('inc')
lib = static_library('sneaky', 'lib.c', include_directories: inc)
meson.override_dependency('sneaky',
  declare_dependency(link_with: lib, include_directories: inc))

subproject('a')
