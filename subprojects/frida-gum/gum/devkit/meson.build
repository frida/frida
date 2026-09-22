devkit_outputs = [
  'frida-gum.h',
  static_lib_prefix + 'frida-gum' + static_lib_suffix,
  'frida-gum-example.c',
]

if cc.get_argument_syntax() == 'msvc'
  devkit_outputs += [
    'frida-gum-example.sln',
    'frida-gum-example.vcxproj',
    'frida-gum-example.vcxproj.filters',
  ]
endif

custom_target('gum-devkit',
  input: gum,
  output: devkit_outputs,
  command: [
    mkdevkit,
    'frida-gum',
    f'@host_os@-@host_arch@@host_config@',
    meson.current_build_dir(),
  ] + devkit_options,
  install: true,
  install_dir: get_option('libdir') / frida_libdir_name / 'devkits' / 'gum'
)
