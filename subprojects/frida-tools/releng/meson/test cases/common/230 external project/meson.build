project('test external project', 'c')

if not find_program('pkg-config', required: false).found()
  error('MESON_SKIP_TEST: pkg-config not found')
endif

if not find_program('make', required : false).found()
  error('MESON_SKIP_TEST: make not found')
endif

if host_machine.system() == 'windows'
  error('MESON_SKIP_TEST: The fake configure script is too dumb to work on Windows')
endif

if meson.is_cross_build()
  # CI uses PKG_CONFIG_SYSROOT_DIR which breaks -uninstalled.pc usage.
  error('MESON_SKIP_TEST: Cross build support is too limited for this test')
endif

pkg = import('pkgconfig')

somelib = library('somelib', 'func.c')
pkg.generate(somelib)

subdir('libfoo')

executable('test-find-library', 'app.c', dependencies : libfoo_dep)
