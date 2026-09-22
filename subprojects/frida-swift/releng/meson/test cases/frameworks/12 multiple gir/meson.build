project('multiple-gobject-introspection', 'c', meson_version: '>=0.50.0')

gir = find_program('g-ir-scanner', required: false)
if not gir.found()
  error('MESON_SKIP_TEST g-ir-scanner not found.')
endif

gnome = import('gnome')
gobj = dependency('gobject-2.0')

subdir('mesongir')
subdir('gir')
