# Language are case unsensitive, check here that capital C works too.
project('valatest', 'vala', 'C')

valadeps = [dependency('glib-2.0'), dependency('gobject-2.0')]

e = executable('valaprog', 'prog.vala', dependencies : valadeps)
test('valatest', e)
