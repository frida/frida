project('sub', 'c')

meson.add_devenv({'TEST_B': '1'})

libfoo = shared_library('foo', 'foo.c')
meson.override_dependency('foo', declare_dependency(link_with: libfoo))
