project('subdir include order', 'c')

# Ensure that headers in subdirs override external dependencies
cc = meson.get_compiler('c')

glib = dependency('glib-2.0')

# This will fail to compile if it picks up the system-installed glib instead of
# the glib in the subdir
executable('prog', 'prog.c',
  include_directories : include_directories('subdir'),
  dependencies : glib)
