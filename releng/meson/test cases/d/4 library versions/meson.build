project('D library versions', 'd')

dc = meson.get_compiler('d')
if dc.get_id() == 'gcc'
  if dc.version().version_compare('< 8')
    error('MESON_SKIP_TEST: GDC < 8.0 cannot build shared libraries')
  endif
endif

shared_library('some', 'lib.d',
  version : '1.2.3',
  soversion : '0',
  install : true)

shared_library('noversion', 'lib.d',
  install : true)

shared_library('onlyversion', 'lib.d',
  version : '1.4.5',
  install : true)

shared_library('onlysoversion', 'lib.d',
  # Also test that int soversion is acceptable
  soversion : 5,
  install : true)
