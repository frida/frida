project('no glib-compile-resources')

if find_program('glib-compile-resources', required: false).found()
  error('MESON_SKIP_TEST test only applicable when glib-compile-resources is missing.')
endif

gnome = import('gnome')
res = gnome.compile_resources('resources', 'trivial.gresource.xml')
