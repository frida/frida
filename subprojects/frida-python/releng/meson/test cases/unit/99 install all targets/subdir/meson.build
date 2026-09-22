configure_file(input: 'foo2.in', output: 'foo2.h',
  configuration: {'foo': 'bar'},
  install_dir: get_option('datadir'),
  install: true,
)
custom_target('ct4',
  output: ['out1.txt', 'out2.txt'],
  command: ['script.py', '@OUTPUT@'],
  install_dir: get_option('datadir'),
  install: true,
)
install_headers('foo3-devel.h')
install_data('bar2-devel.h',
  install_dir: get_option('includedir'),
)
executable('app2', 'main.c',
  install: true,
)
both_libraries('both2', 'lib.c',
  install: true,
)
