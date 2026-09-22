project('made up', 'c', meson_version : '>= 1.3.0')

s = static_library('lib', 'lib.c')

dep = declare_dependency(link_with : s, include_directories : '.')

meson.override_dependency('made up', dep)
