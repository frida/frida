project('test installs')

install_emptydir('usr/share/foo')
install_headers('header.h')
install_man('man.1')
install_data('header.h', install_dir : 'foo')
install_symlink('link', install_dir : 'usr/bin', pointing_to : '/usr/bin/testprog')
install_subdir('subdir', install_dir : 'share')

meson.add_install_script('script.py')
meson.add_postconf_script('script.py')
