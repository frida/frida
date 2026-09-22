project('find fortran library', 'fortran')

fc = meson.get_compiler('fortran')

sources = ['main.f90', 'gzip.f90']
zlib = fc.find_library('z', required: false)

if not zlib.found()
  error('MESON_SKIP_TEST: Z library not available.')
endif

exe = executable('zlibtest', sources, dependencies : zlib)
test('testzlib', exe)
