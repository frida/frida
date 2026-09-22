project('pkgformat', 'c',
  version : '1.0')

gio = dependency('gio-2.0', required: false)
if not gio.found()
  error('MESON_SKIP_TEST glib not found.')
endif

pkgg = import('pkgconfig')

s = static_library('returner', 'someret.c')
l = library('something', 'somelib.c', link_whole: s)

pkgg.generate(libraries: l,
              version: '1.0',
              name: 'libsomething',
              description: 'A library that does something',
              requires: 'gobject-2.0 >= 2.0, gio-2.0 >= 2.0')
