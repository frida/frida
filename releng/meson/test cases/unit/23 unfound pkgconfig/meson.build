project('foobar', 'c')

unfound = dependency('blub_blob_blib', required : false)

pkgg = import('pkgconfig')

l = shared_library('somename', 'some.c',
  dependencies : unfound)

pkgg.generate(
  libraries : l,
  name : 'somename',
  version : '1.0.0',
  description : 'A test library.',
)
