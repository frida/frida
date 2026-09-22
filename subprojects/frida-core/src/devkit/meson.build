devkit_outputs = [
  'frida-core.gir',
  'frida-core.h',
  static_lib_prefix + 'frida-core' + static_lib_suffix,
  'frida-core-example.c',
]

if host_toolchain == 'microsoft'
  devkit_outputs += [
    'frida-core-example.sln',
    'frida-core-example.vcxproj',
    'frida-core-example.vcxproj.filters',
  ]
endif

custom_target('core-devkit',
  input: [core_public_h, core_public_vapi, core_public_lib],
  output: devkit_outputs,
  command: [
    mkdevkit,
    'frida-core',
    f'@host_os@-@host_arch@',
    meson.current_build_dir(),
  ] + devkit_options,
  install: true,
  install_dir: get_option('libdir') / frida_libdir_name / 'devkits' / 'core'
)
