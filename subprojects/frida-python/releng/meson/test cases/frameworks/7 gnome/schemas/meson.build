
compiled = gnome.compile_schemas(build_by_default: true)
install_data('com.github.meson.gschema.xml',
install_dir : 'share/glib-2.0/schemas')

schemaexe = executable('schemaprog', 'schemaprog.c', dependencies : gio)
test('schema test', schemaexe)
