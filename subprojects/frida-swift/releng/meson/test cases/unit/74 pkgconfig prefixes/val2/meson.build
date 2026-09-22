project('val2', 'c')
val1_dep = dependency('val1')
val2 = shared_library('val2', 'val2.c',
  dependencies : [val1_dep],
  install: true)
install_headers('val2.h')
pkgconfig = import('pkgconfig')
pkgconfig.generate(val2, libraries : ['-Wl,-rpath,${libdir}'])
