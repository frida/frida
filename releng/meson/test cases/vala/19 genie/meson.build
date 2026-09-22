project('genietest', 'vala', 'c')

valadeps = [dependency('glib-2.0'), dependency('gobject-2.0')]

e = executable('genieprog', 'prog.gs', dependencies : valadeps)
test('genietest', e)
