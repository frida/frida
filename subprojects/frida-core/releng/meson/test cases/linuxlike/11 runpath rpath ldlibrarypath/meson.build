project('runpath rpath ldlibrarypath', 'c')

libsrc = files('lib.c')

subdir('lib1')
subdir('lib2')

lib2dir = meson.current_build_dir() + '/lib2'

e = executable('testexe', 'main.c',
  link_with : lib1)

test('ld-library-path-test', e,
  env : ['LD_LIBRARY_PATH=' + lib2dir])
