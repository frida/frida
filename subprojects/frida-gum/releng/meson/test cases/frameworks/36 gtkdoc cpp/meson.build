project('gnome module without C', 'cpp')

gtkdoc = find_program('gtkdoc-scan', required: false)
if not gtkdoc.found()
  error('MESON_SKIP_TEST gtkdoc not found.')
endif

gnome = import('gnome')

lib = library('foo++', 'foo.cpp')
gnome.gtkdoc('foo',
  src_dir: '.',
  main_xml : 'foo-docs.xml',)
