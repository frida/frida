project('find library and headers', 'c')

cc = meson.get_compiler('c')

if not cc.find_library('z', required : false).found()
  error('MESON_SKIP_TEST: zlib not found.')
endif

lib = cc.find_library('z',
  has_headers : 'foo.h',
  required : false)
assert(not lib.found(), 'Header should be missing')

lib = cc.find_library('z',
  has_headers : 'foo.h',
  header_include_directories : include_directories('.'))
assert(lib.found(), 'Header should be found')

lib = cc.find_library('z',
  has_headers : ['foo.h', 'bar.h'],
  header_include_directories : include_directories('.'),
  required : false)
assert(not lib.found(), 'One header should be missing')
