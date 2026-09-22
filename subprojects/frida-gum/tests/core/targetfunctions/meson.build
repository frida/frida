project('targetfunctions', 'c')

host_os_family = host_machine.system()

glib_dep = dependency('glib-2.0')

targetfunctions_link_args = []
targetfunctions_link_depends = []

specialfunctions_link_args = []
specialfunctions_link_depends = []

if host_os_family == 'darwin'
  symlist = 'targetfunctions.symbols'
  targetfunctions_link_args += ['-Wl,-exported_symbols_list,' + meson.current_source_dir() / symlist]
  targetfunctions_link_depends += [symlist]

  symlist = 'specialfunctions.symbols'
  specialfunctions_link_args += ['-Wl,-exported_symbols_list,' + meson.current_source_dir() / symlist]
  specialfunctions_link_depends += [symlist]
elif host_os_family != 'windows'
  symscript = 'targetfunctions.version'
  targetfunctions_link_args += ['-Wl,--version-script,' + meson.current_source_dir() / symscript]
  targetfunctions_link_depends += [symscript]

  symscript = 'specialfunctions.version'
  specialfunctions_link_args += ['-Wl,--version-script,' + meson.current_source_dir() / symscript]
  specialfunctions_link_depends += [symscript]
endif

stem_suffix = get_option('stem_suffix')

shared_module('targetfunctions' + stem_suffix, 'targetfunctions.c',
  name_prefix: '',
  override_options: ['optimization=0'],
  dependencies: [glib_dep],
  link_args: targetfunctions_link_args,
  link_depends: targetfunctions_link_depends,
  install: true,
)

shared_module('specialfunctions' + stem_suffix, 'specialfunctions.c',
  name_prefix: '',
  override_options: ['optimization=2'],
  dependencies: [glib_dep],
  link_args: specialfunctions_link_args,
  link_depends: specialfunctions_link_depends,
  install: true,
)
