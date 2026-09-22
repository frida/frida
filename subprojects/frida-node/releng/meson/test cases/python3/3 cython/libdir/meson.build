pyx_c = custom_target('storer_pyx',
  output : 'storer_pyx.c',
  input : 'storer.pyx',
  depend_files : 'cstorer.pxd',
  command : [cython, '@INPUT@', '-o', '@OUTPUT@'],
)

slib = py3_mod.extension_module('storer',
  'storer.c', pyx_c,
  dependencies : py3_dep)

pydir = meson.current_build_dir()
