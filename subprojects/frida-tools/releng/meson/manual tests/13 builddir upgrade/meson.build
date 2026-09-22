project('check old builddirs in a stable release', 'c')

lib = both_libraries('lib', 'lib.c')
exe = executable('foo', 'foo.c', link_with: lib, install: true)
test('exe', exe)

install_data('data/foo.dat', install_dir: get_option('datadir') / 'foo')
install_man('foo.1')

py = import('python').find_installation()
py.install_sources('mod.py', subdir: 'foo')
install_subdir('data', install_dir: py.get_install_dir())

custom_target(
  input: 'mod.py',
  output: 'hello.dat',
  command: [py, '@INPUT@'],
  capture: true,
  install: true,
  install_dir: get_option('localstatedir') / 'foo',
)
