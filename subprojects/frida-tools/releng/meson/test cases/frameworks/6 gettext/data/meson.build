# Use filename substitution
i18n.merge_file(
  input: 'test.desktop.in',
  output: '@BASENAME@',
  type: 'desktop',
  po_dir: '../po',
  data_dirs: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

# Use filename substitution for another file
i18n.merge_file(
  input: files('test2.desktop.in'),
  output: '@BASENAME@',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

i18n.merge_file(
  input: 'test.desktop.in',
  output: 'test3.desktop',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

# Regression test when passing File object as input and '@BASENAME@' as output
# in multiple i18n.merge_file() calls. It used to make target name collision.
# https://github.com/mesonbuild/meson/issues/9022
i18n.merge_file(
  input: configure_file(
    input: 'test5.desktop.in.in',
    output: '@BASENAME@',
    configuration: { 'NAME': 'Application' },
  ),
  output: '@BASENAME@',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)
i18n.merge_file(
  input: configure_file(
    input: 'test6.desktop.in.in',
    output: '@BASENAME@',
    configuration: { 'NAME': 'Application' },
  ),
  output: '@BASENAME@',
  type: 'desktop',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(get_option('datadir'), 'applications')
)

subdir('data3')
