extra_link_args = []
extra_link_depends = []

if host_os_family == 'darwin'
  symlist = 'runner.symbols'
  extra_link_args += '-Wl,-exported_symbols_list,' + meson.current_source_dir() / symlist
  extra_link_depends += symlist
elif host_os_family != 'windows'
  symscript = 'runner.version'
  extra_link_args += '-Wl,--version-script,' + meson.current_source_dir() / symscript
  extra_link_depends += [symscript]
endif

shared_module('testswiftapiresolver', 'runner.c',
  dependencies: [gum_dep],
  link_args: extra_link_args,
  link_depends: extra_link_depends,
)
