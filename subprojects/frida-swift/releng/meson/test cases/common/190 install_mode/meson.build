project('install_mode test', 'c',
  default_options : ['install_umask=027', 'libdir=libtest'])

if build_machine.system() == 'windows'
  error('MESON_SKIP_TEST: install_mode test requires a Unix-like OS')
endif

# confirm no regressions in install_data
install_data('runscript.sh',
  install_dir : get_option('bindir'),
  install_mode : ['rwxr-sr-x', 'root', 0])

# confirm no regressions in install_subdir
install_subdir('sub1',
  install_dir : 'share',
  install_mode : ['rwxr-x--t', 'root'])

install_subdir('sub2',
  install_dir : 'share')

# test install_mode in configure_file
conf = configuration_data()
conf.set('var', 'mystring')
conf.set('other', 'string 2')
conf.set('second', ' bonus')
conf.set('BE_TRUE', true)
configure_file(input : 'config.h.in',
  output : 'config.h',
  configuration : conf,
  install_dir : 'include',
  install_mode : 'rw-rwSr--')

# test install_mode in custom_target
custom_target('bindat',
  output : 'data.dat',
  input : 'data_source.txt',
  command : ['cp', '@INPUT@', '@OUTPUT@'],
  install : true,
  install_dir : 'subdir',
  install_mode : 'rw-rwSr--')

# test install_mode in install_headers
install_headers('rootdir.h',
  install_mode : 'r--r--r-T')

# test install_mode in install_man
install_man('foo.1',
  install_mode : 'r--r--r-T')

# test install_mode in executable
executable('trivialprog',
  sources : 'trivial.c',
  install : true,
  build_rpath: meson.current_build_dir(),
  install_mode : ['rwxr-sr-x', 'root', 'root'])

# test install_mode in static_library
static_library('stat', 'stat.c',
  install : true,
  install_mode : ['rw---Sr--'])
