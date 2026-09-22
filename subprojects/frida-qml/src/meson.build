python = import('python').find_installation()
qt = import('qt6')

sources = [
  'plugin.cpp',
  'device.cpp',
  'application.cpp',
  'process.cpp',
  'maincontext.cpp',
  'frida.cpp',
  'spawnoptions.cpp',
  'script.cpp',
  'devicelistmodel.cpp',
  'applicationlistmodel.cpp',
  'processlistmodel.cpp',
  'iconprovider.cpp',
  'variant.cpp',
]

moc_sources = qt.compile_moc(
  headers: [
    'plugin.h',
    'device.h',
    'application.h',
    'process.h',
    'frida.h',
    'spawnoptions.h',
    'script.h',
    'devicelistmodel.h',
    'applicationlistmodel.h',
    'processlistmodel.h',
  ],
  dependencies: [qt_dep],
  extra_args: [
    '-Muri=Frida',
    '--output-json',
  ],
)

plugin_dir = get_option('libdir') / 'qml' / 'Frida'

install_data('qmldir', install_dir: plugin_dir)

qmltypes = custom_target('qmltypes',
  input: moc_sources,
  output: [
    'qmltyperegistrations.cpp',
    'plugins.qmltypes',
  ],
  command: [
    python,
    files('generate-qmltypes.py'),
    '@OUTPUT@',
    '@PRIVATE_DIR@',
    qt_dep.get_variable('prefix', configtool: 'QT_HOST_PREFIX'),
    qt_dep.get_variable('libdir', configtool: 'QT_HOST_LIBS'),
    qt_dep.get_variable('libexecdir', configtool: 'QT_HOST_LIBEXECS'),
    '@INPUT@',
  ],
  install: true,
  install_dir: [false, plugin_dir],
)

extra_link_args = []
extra_link_depends = []
if host_machine.system() == 'darwin'
  extra_link_args += [
    '-Wl,-exported_symbol,_qt_plugin_query_metadata_v2',
    '-Wl,-exported_symbol,_qt_plugin_instance',
  ]
elif cpp.get_argument_syntax() != 'msvc'
  symscript = 'frida-qml.version'
  extra_link_args += '-Wl,--version-script,' + meson.current_source_dir() / symscript
  extra_link_depends += symscript
endif

shared_module('frida-qml', sources, moc_sources, qmltypes,
  link_args: extra_link_args,
  link_depends: extra_link_depends,
  dependencies: [qt_dep, frida_core_dep],
  install: true,
  install_dir: plugin_dir,
)
