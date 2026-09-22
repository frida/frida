frida_private_depends = []
if frida_core_dep.type_name() == 'internal'
  # XXX: Cannot get our custom_target() to depend on frida_core_dep itself
  frida_private_depends += subproject('frida-core').get_variable('core_api')
endif

frida_private_modulemap = custom_target('modulemap',
  output: 'module.modulemap',
  command: [
    python,
    files('generate-modulemap.py'),
    '@OUTPUT@',
    frida_core_dep.get_variable('frida_includedir') / 'frida-core.h',
  ],
  depends: frida_private_depends,
)

frida_private_dep = declare_dependency(
  sources: frida_private_modulemap,
  dependencies: frida_core_dep,
)
