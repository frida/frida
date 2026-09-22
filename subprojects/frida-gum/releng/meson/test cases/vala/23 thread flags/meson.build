project('thread flags', 'c', 'vala')

glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')
threads = dependency('threads')

e = executable('valaprog', 'prog.vala', dependencies: [glib, gobject, threads])
