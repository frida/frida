sources = [
  'DeviceManager.cpp',
  'Device.cpp',
  'Process.cpp',
  'Session.cpp',
  'Script.cpp',
  'Runtime.cpp',
  'Marshal.cpp',
]

shared_module('Frida', sources,
  override_options: 'cpp_eh=a',
  cpp_args: [
    '/clr',
    '/I' + (netfx_sdk / 'Include' / 'um'),
    '/AI' + netfx_runtime,
    '/FU' + (netfx_runtime / 'mscorlib.dll'),
  ],
  link_args: [
    '/LIBPATH:' + (netfx_sdk / 'lib' / 'um' / host_msvc_platform),
    '/IGNORE:4248',
  ],
  dependencies: frida_core_dep,
  install: true,
  install_dir: get_option('bindir'),
)
