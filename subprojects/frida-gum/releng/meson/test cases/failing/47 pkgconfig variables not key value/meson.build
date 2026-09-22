project('variables-not-key-value-test', 'c', version : '1.0')

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
  variables : [ 'this_should_be_key_value' ]
)
