project('D Shared Library', 'd')

dc = meson.get_compiler('d')
if dc.get_id() == 'gcc'
  if dc.version().version_compare('< 8')
    error('MESON_SKIP_TEST: GDC < 8.0 cannot build shared libraries')
  endif
endif

subdir('sub')
ed = executable('app_d', 'app.d', link_with : ldyn, install : true)
test('linktest_dyn', ed)

# test D attributes for pkg-config
pkgc = import('pkgconfig')
pkgc.generate(name: 'test',
          libraries: ldyn,
          subdirs: 'd/stuff',
          description: 'A test of D attributes to pkgconfig.generate.',
          d_module_versions: ['Use_Static']
)

ldd = find_program('ldd', required : false)
if ldd.found()
  test('ldd-test.py', ed)
endif
