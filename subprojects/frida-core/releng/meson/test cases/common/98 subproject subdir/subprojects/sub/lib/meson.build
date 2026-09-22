lib = static_library('sub', 'sub.c')
libSub = declare_dependency(include_directories: include_directories('.'), link_with: lib)
meson.override_dependency('sub-1.0', libSub)
