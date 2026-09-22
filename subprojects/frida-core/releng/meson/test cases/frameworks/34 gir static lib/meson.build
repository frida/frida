project('gobject-introspection-static-helper', 'c')

gir = find_program('g-ir-scanner', required: false)
if not gir.found()
  error('MESON_SKIP_TEST g-ir-scanner not found.')
endif

gobject_introspection = dependency('gobject-introspection-1.0')
# This won't work without https://gitlab.gnome.org/GNOME/gobject-introspection/merge_requests/72
if gobject_introspection.version().version_compare('< 1.58.1')
  error('MESON_SKIP_TEST gobject-introspection is too old to support static libraries')
endif

gnome = import('gnome')
gobj = dependency('gobject-2.0')

subdir('statichelper')
subdir('subdir/gir')
