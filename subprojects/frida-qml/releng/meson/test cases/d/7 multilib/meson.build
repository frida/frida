project('D Multiple Versioned Shared Libraries', 'd')

dc = meson.get_compiler('d')
if dc.get_id() == 'gcc'
  if dc.version().version_compare('< 8')
    error('MESON_SKIP_TEST: GDC < 8.0 cannot build shared libraries')
  endif
endif

ldyn1 = shared_library('say1',
    'say1.d',
    install: true,
    version : '1.2.3',
    soversion : '0'
)
ldyn2 = shared_library('say2',
    'say2.d',
    install: true,
    version : '1.2.4',
    soversion : '1'
)

ed = executable('app_d', 'app.d', link_with: [ldyn1, ldyn2], install: true)
test('multilink_test', ed)
