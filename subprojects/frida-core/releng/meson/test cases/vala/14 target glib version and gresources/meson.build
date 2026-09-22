project('test glib target version and gresources', 'c', 'vala')

gnome = import('gnome')

glib = dependency('glib-2.0', version : '>=2.38')
gtk = dependency('gtk+-3.0')

subdir('gres')

e = executable('gtktemplate', 'test.vala', res, dependencies : [glib, gtk])
# No X on the CI, so disable this for now
#test('test-target-glib', e)
