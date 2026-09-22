pyx_c = custom_target('storer_pyx',
  output : 'storer_pyx.c',
  input : 'storer.pyx',
  command : [cython, '@INPUT@', '-o', '@OUTPUT@', '-3'],
)

slib = py3.extension_module('storer',
  'storer.c', pyx_c,
)

pydir = meson.current_build_dir()
