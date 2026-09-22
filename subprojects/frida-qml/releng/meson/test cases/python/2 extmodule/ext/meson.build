pylib = py.extension_module('tachyon',
  'tachyon_module.c',
  c_args: '-DMESON_MODULENAME="tachyon"',
  install: true,
)

pylib2 = py2.extension_module('tachyon',
  'tachyon_module.c',
  c_args: '-DMESON_MODULENAME="tachyon"',
  install: true,
)

subdir('nested')
subdir('wrongdir')
pypathdir = meson.current_build_dir()
