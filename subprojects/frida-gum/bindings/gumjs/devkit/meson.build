devkit_outputs = [
  'frida-gumjs.h',
  static_lib_prefix + 'frida-gumjs' + static_lib_suffix,
  'frida-gumjs-example.c',
]

if cc.get_argument_syntax() == 'msvc'
  devkit_outputs += [
    'frida-gumjs-example.sln',
    'frida-gumjs-example.vcxproj',
    'frida-gumjs-example.vcxproj.filters',
  ]
endif

custom_target('gumjs-devkit',
  input: [gum, gum_heap, gum_prof, gumjs],
  output: devkit_outputs,
  command: [
    mkdevkit,
    'frida-gumjs',
    f'@host_os@-@host_arch@@host_config@',
    meson.current_build_dir(),
  ] + devkit_options,
  install: true,
  install_dir: get_option('libdir') / frida_libdir_name / 'devkits' / 'gumjs'
)
