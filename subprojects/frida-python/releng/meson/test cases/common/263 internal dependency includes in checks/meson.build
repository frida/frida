project('test 262', 'c')

cc = meson.get_compiler('c')

internal_dep = declare_dependency(include_directories: 'include')

assert(cc.has_header_symbol('test_262_header.h', 'foo', dependencies: internal_dep))
