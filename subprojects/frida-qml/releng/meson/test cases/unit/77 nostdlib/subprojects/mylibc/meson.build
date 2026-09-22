project('own libc', 'c')

# A very simple libc implementation

# Do not specify -nostdlib & co. They come from cross specifications.

libc = static_library('c', 'libc.c', 'stubstart.s')

mylibc_dep = declare_dependency(link_with : libc,
  include_directories : include_directories('.')
)

meson.override_dependency('c_stdlib', mylibc_dep)
