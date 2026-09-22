project('variables-zero-length-test', 'c', version : '1.0')

pkgg = import('pkgconfig')
lib = shared_library('simple', 'simple.c')
libver = '1.0'
h = install_headers('simple.h')

pkgg.generate(
  libraries : [lib, '-lz'],
  subdirs : '.',
  version : libver,
  name : 'libsimple',
  filebase : 'simple',
  description : 'A simple demo library.',
  variables : [ '=value' ]
)
