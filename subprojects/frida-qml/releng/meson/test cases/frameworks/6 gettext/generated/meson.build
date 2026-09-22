dgen = find_program('desktopgenerator.py')

desktop_in_file = custom_target('something.desktop.in',
  input : ['something.desktop.in.in'],
  output : 'something.desktop.in',
  command : [dgen, '@INPUT@', '@OUTPUT@'],
)

i18n.merge_file(
  input : desktop_in_file,
  output : 'something.desktop',
  type : 'desktop',
  po_dir : '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications'),
)
