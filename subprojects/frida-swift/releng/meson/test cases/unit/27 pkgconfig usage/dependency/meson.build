project('pkgconfig dep', 'c',
  version : '1.0.0')

# This is not used in the code, only to check that it does not
# leak out to --libs.
glib_dep = dependency('glib-2.0')

pkgconfig = import('pkgconfig')

intlib = static_library('libpkgdep-int', 'privatelib.c')
intdep = declare_dependency(link_with : intlib)

lib = shared_library('pkgdep', 'pkgdep.c',
  dependencies : [glib_dep, intdep],
  install : true)

install_headers('pkgdep.h')

pkgconfig.generate(
  filebase : 'libpkgdep',
  name : 'Libpkgdep',
  description : 'Sample pkgconfig dependency library',
  version : meson.project_version(),
  libraries : lib)
