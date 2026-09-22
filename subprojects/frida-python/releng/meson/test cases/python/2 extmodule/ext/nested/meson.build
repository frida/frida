py.extension_module('tachyon',
  '../tachyon_module.c',
  c_args: '-DMESON_MODULENAME="nested.tachyon"',
  install: true,
  subdir: 'nested'
)
py.install_sources(
  configure_file(
    input: '../../blaster.py.in',
    output: 'blaster.py',
    configuration: {'tachyon_module': 'nested.tachyon'}
  ),
  pure: false,
  subdir: 'nested',
)


py2.extension_module('tachyon',
  '../tachyon_module.c',
  c_args: '-DMESON_MODULENAME="nested.tachyon"',
  install: true,
  subdir: 'nested'
)
py2.install_sources(
  configure_file(
    input: '../../blaster.py.in',
    output: 'blaster.py',
    configuration: {'tachyon_module': 'nested.tachyon'}
  ),
  pure: false,
  subdir: 'nested',
)
