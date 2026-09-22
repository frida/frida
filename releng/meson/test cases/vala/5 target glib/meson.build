project('valatest', 'vala', 'c')

valadeps = [dependency('glib-2.0', version : '>=2.32'), dependency('gobject-2.0')]

e = executable('valaprog', 'GLib.Thread.vala', 'retcode.c', dependencies : valadeps)
test('valatest', e)
