types = configure_file(input: '../foobar1/foobar.types',
  output: 'foobar.types',
  copy: true
)

gnome.gtkdoc('foobar2',
  src_dir : inc,
  main_sgml : 'foobar-docs.sgml',
  content_files : [docbook, version_xml],
  gobject_typesfile: types,
  dependencies: foo_dep,
  install : true,
  install_dir : 'foobar2')
