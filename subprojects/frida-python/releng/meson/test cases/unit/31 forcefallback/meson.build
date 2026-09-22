project('mainproj', 'c',
  default_options : [])

zlib_dep = dependency('zlib', fallback: ['notzlib', 'zlib_dep'])
notfound_dep = dependency('cannotabletofind', fallback: ['definitelynotfound', 'some_var'], required : false)

test_not_zlib = executable('test_not_zlib', ['test_not_zlib.c'], dependencies: [zlib_dep, notfound_dep])

test('test_not_zlib', test_not_zlib)
