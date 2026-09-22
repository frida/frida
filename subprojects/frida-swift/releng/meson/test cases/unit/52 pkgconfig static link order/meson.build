project('link order test', 'c')

dep = library('dependency', 'dummy.c')
lib = static_library('something', 'dummy.c', link_with: dep)

import('pkgconfig').generate(
  name: 'libsomething',
  description: 'test library',
  libraries: lib,
  version: '1'
)
