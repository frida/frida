# adding 'c' shouldn't be required
project('multiple files')
add_languages('vala')

glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')

e = executable('prog', 'main.vala', 'class1.vala', 'class2.vala',
dependencies : [glib, gobject])
test('multiple file test', e)
