project('downloader', 'c')

cc = meson.get_compiler('c')

s = subproject('sqlite').get_variable('sqlite_dep')
th = dependency('threads')

libdl = cc.find_library('dl', required : false)

e = executable('dtest', 'main.c',
  dependencies : [th, libdl, s])

test('dltest', e)
