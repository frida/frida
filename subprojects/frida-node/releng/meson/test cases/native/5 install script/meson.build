project('custom install script', 'c')

# this is just to ensure that the install directory exists before exe is run
install_data('file.txt', install_dir: '.')

subdir('src')

meson.add_install_script(exe, 'generated.txt')
wrap = find_program('wrap.py')
# Yes, these are getting silly
meson.add_install_script(wrap, exe, 'wrapped.txt')
meson.add_install_script(wrap, wrap, exe, 'wrapped2.txt')
