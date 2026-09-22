inject_sources = [
  'inject.vala',
  'inject-glue.c',
]

system_vala_args = []
if host_os_family != 'windows'
  system_vala_args += ['--pkg=posix']
endif

raw_inject = executable('frida-inject-raw', inject_sources,
  vala_args: [core_vala_args, system_vala_args],
  link_args: executable_no_exports_link_args,
  link_depends: executable_no_exports_link_depends,
  dependencies: [json_glib_dep, core_dep],
)

custom_target('frida-inject',
  input: [raw_inject, 'frida-inject.xcent'],
  output: 'frida-inject' + exe_suffix,
  command: post_process + ['executable', 're.frida.Inject', '@INPUT1@'],
  install: true,
  install_dir: get_option('bindir'),
)
