png = configure_file(input: 'baz.png.in',
  output: 'baz.png',
  copy: true)

gnome.gtkdoc('foobar',
  src_dir : [inc, '.'],
  main_sgml : 'foobar-docs.sgml',
  content_files : [docbook, version_xml],
  dependencies: foo_dep,
  html_assets: ['baz.jpg', png],
  # Manually written types file for regression test:
  # https://github.com/mesonbuild/meson/issues/8744
  gobject_typesfile: 'foobar.types',
  install : true,
  check: false)
