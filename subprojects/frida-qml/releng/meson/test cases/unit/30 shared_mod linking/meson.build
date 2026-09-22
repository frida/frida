project('shared library linking test', 'c', 'cpp')

mod = shared_module('mymod', 'libfile.c')

exe = executable('prog', 'main.c', link_with : mod, install : true)