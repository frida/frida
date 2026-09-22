project('lddedup', 'c')

# Chosen because its ldflags contains -Wl,--export-dynamic,
# which must be deduplicated.
gm = dependency('gmodule-2.0')

lib = static_library('bob', 'bob.c',
  dependencies: gm)

executable('prog', 'prog.c',
  link_with: lib,
  dependencies: gm)
