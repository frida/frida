project('foo', 'c')

install_data('foo.dat')
install_headers('foo.h')
install_subdir('foo', install_dir: '')
executable('foo', 'foo.c', install: true)

subproject('bar')
