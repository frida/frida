portal_sources = [
  'portal.vala',
]

extra_deps = []
if tls_provider_dep.found()
  extra_deps += tls_provider_dep
endif

raw_portal = executable('frida-portal-raw', portal_sources,
  vala_args: [core_vala_args, tls_provider_vala_args, '--pkg=posix'],
  link_args: executable_no_exports_link_args,
  link_depends: executable_no_exports_link_depends,
  dependencies: [core_dep] + extra_deps,
)

custom_target('frida-portal',
  input: [raw_portal, 'frida-portal.xcent'],
  output: 'frida-portal' + exe_suffix,
  command: post_process + ['executable', 're.frida.Portal', '@INPUT1@'],
  install: true,
  install_dir: get_option('bindir'),
)
