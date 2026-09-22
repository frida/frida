project(
  'cmake config file',
)

cmake = import('cmake')

cmake.configure_package_config_file(
  name : 'foolib',
  input : 'foolib.cmake.in',
  install_dir : get_option('libdir') / 'cmake',
  configuration : {'foo': '"bar"'},
)
