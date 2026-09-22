project('install_emptydir')

if build_machine.system() == 'windows' and meson.backend() == 'ninja'
  error('MESON_SKIP_TEST windows does not support symlinks unless root or in development mode')
endif

install_data('datafile.dat', install_dir: 'share/progname/C')
install_symlink('datafile.dat', pointing_to: '../C/datafile.dat', install_dir: 'share/progname/es')
install_symlink('rename_datafile.dat', pointing_to: '../C/datafile.dat', install_dir: 'share/progname/fr')
