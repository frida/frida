project('giotest', 'vala', 'c')

glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')
gio = [dependency('gio-2.0'),
       # https://github.com/mesonbuild/meson/issues/1484
       dependency('gio-unix-2.0', required : false),
       dependency('gio-windows-2.0', required : false)]

e = executable('gioprog', 'gioprog.vala',
dependencies : [glib, gobject, gio])
test('giotest', e)
