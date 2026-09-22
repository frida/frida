project('identity cross test', 'c')

assert(meson.get_compiler('c', native: true).get_define(
  'GOT',
  args : [ '-DARG_BUILD' ],
  prefix : '#include "stuff.h"',
  include_directories: include_directories('.'),
) == 'BUILD', 'did not get BUILD from native: true compiler')

assert(meson.get_compiler('c', native: false).get_define(
  'GOT',
  args : [ '-DARG_HOST' ],
  prefix : '#include "stuff.h"',
  include_directories: include_directories('.'),
) == 'HOST', 'did not get HOST from native: false compiler')
