project('wintest', 'c')

# Test that we can produce an implib for an executable on Windows, and that it's
# name can be set, and that it is installed along with the executable

executable('prog', 'prog.c', install: true, implib: true)
executable('prog2', 'prog.c', install: true, implib: 'burble', install_dir: get_option('bindir'))
