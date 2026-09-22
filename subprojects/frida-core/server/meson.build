server_sources = [
  'server.vala',
  'server-glue.c',
]

if host_os_family == 'darwin'
  server_sources += ['server-darwin.m']
endif
if host_os in ['ios', 'tvos', 'xros']
  server_sources += [
    'server-ios-tvos.c',
  ]
endif

extra_link_args = []
extra_link_depends = []
if host_os_family == 'darwin'
  symlist = 'frida-server.symbols'
  extra_link_args += ['-Wl,-exported_symbols_list,' + meson.current_source_dir() / symlist]
  extra_link_depends += [symlist]
elif host_os_family != 'windows'
  symscript = (host_os == 'freebsd') ? 'frida-server-freebsd.version' : 'frida-server.version'
  extra_link_args += ['-Wl,--version-script,' + meson.current_source_dir() / symscript]
  extra_link_depends += [symscript]
endif
if host_os == 'macos'
  server_plist = 'frida-server.plist'
  extra_link_args += ['-Wl,-sectcreate,__TEXT,__info_plist,' + meson.current_source_dir() / server_plist]
  extra_link_depends += [server_plist]
endif

server_deps = [core_dep]
if host_os_family == 'darwin'
  server_deps += dependency('appleframeworks', modules: ['CoreFoundation'])
endif

raw_server = executable('frida-server-raw', server_sources,
  vala_args: [core_vala_args, '--pkg=posix'],
  link_args: extra_link_args,
  link_depends: extra_link_depends,
  dependencies: server_deps,
)

server_name = 'frida-server' + exe_suffix
need_universal_server = assets_mode == 'installed' and server_compat != ''

server = custom_target('frida-server',
  input: [raw_server, 'frida-server.xcent'],
  output: need_universal_server ? f'@server_name@-@host_abi@' : server_name,
  command: post_process + ['executable', 're.frida.Server', '@INPUT1@'],
  install: true,
  install_dir: get_option('bindir'),
)

if need_universal_server
  custom_target('frida-server-universal',
    input: server,
    output: server_name,
    command: [
      lipo,
      '@INPUT@',
      server_compat,
      '-create',
      '-output', meson.current_build_dir() / server_name
    ],
    depends: server_depends,
    install: true,
    install_dir: get_option('bindir'),
  )
endif
