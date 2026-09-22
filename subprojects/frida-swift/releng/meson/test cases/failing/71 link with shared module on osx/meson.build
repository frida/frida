project('link with shared module', 'c')

if host_machine.system() != 'darwin'
  error('MESON_SKIP_TEST test only fails on OSX')
endif

m = shared_module('mymodule', 'module.c')
e = executable('prog', 'prog.c', link_with : m)
