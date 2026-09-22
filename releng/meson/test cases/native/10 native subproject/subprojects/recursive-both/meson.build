project('recursive-both', 'c')

fs = import('fs')

header = fs.copyfile('recursive-both.h.in', 'recursive-both.h')

lib = library('lib', 'lib.c', header, vs_module_defs : 'lib.def')

dep = declare_dependency(link_with : lib, sources : header, include_directories : '.')

meson.override_dependency('recursive-both', dep)
