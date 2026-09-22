ct2 = custom_target(
  'ct2',
  input : 'gen.py',
  output : 'ct2.pyx',
  command : [py3, '@INPUT@', '@OUTPUT@'],
)

ct2_ext = py3.extension_module('ct2', ct2, dependencies : py3_dep)

pydir = meson.current_build_dir()
