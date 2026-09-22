project('foo', 'c', 'vala')

glib = dependency('glib-2.0')

subdir('c')
e = executable('foo', 'c/foo.c', retval, 'vala/bar.vala', dependencies: [glib])
test('test foo', e)
