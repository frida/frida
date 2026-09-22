base_sources = [
  'asset-location.vala',
  'session.vala',
  'session-glue.c',
  'channel.vala',
  'service.vala',
  'stream.vala',
  'rpc.vala',
  'p2p.vala',
  'p2p-glue.c',
  'dbus.vala',
  'promise.vala',
  'fs.vala',
  'socket.vala',
  'socket-glue.c',
  'buffer.vala',
  'buffer-glue.c',
  'value.vala',
  'xnu-syscalls' / 'xnu-syscalls.vala',
  'xnu-syscalls' / 'xnu-syscalls-glue.c',
]

extra_vala_args = []

if host_os == 'windows'
  base_sources += 'windows.vala'
else
  base_sources += 'posix.vala'
  extra_vala_args += '--pkg=posix'
endif

if host_os == 'macos'
  base_sources += [
    'xpc.vala',
    'frida-darwin.c',
  ]
endif

if host_os_family == 'darwin'
  base_sources += 'corefoundation.vala'
  extra_vala_args += '--pkg=darwin-xnu'
endif

if host_os_family == 'linux'
  base_sources += [
    'linux.vala',
  ]
  extra_vala_args += [
    '--pkg=linux',
    '--pkg=frida-atomics',
  ]
  if have_local_backend
    extra_vala_args += '--pkg=libbpf'
  endif

  syscall_arch_from_abi = {
    'armbe': 'arm',
    'armbe8': 'arm',
    'armhf': 'arm',
    'armv6kz': 'arm',
    'arm64be': 'arm64',
    'arm64beilp32': 'arm64',
    'mipsel': 'mips',
    'mips64el': 'mips64',
  }
  syscall_arch = syscall_arch_from_abi.get(host_abi, host_abi)
  base_sources += [
    'linux-syscalls' / (syscall_arch + '.vala'),
    'linux-syscalls' / (syscall_arch + '-glue.c'),
  ]
endif

if host_os_family == 'freebsd'
  extra_vala_args += '--pkg=freebsd'
endif

extra_deps = []
if host_os_family != 'windows'
  extra_deps += gio_unix_dep
endif
if host_os_family == 'linux' and have_local_backend
  extra_deps += libbpf_dep
endif

base_vala_args = [gum_vala_args, '--pkg=gio-2.0', '--pkg=json-glib-1.0']
if nice_dep.found()
  base_vala_args += '--pkg=nice'
endif
if host_os_family != 'windows'
  base_vala_args += [
    '--pkg=gio-unix-2.0',
  ]
endif
if host_os_family == 'darwin'
  base_vala_args += [
    '--pkg=corefoundation',
    '--pkg=darwin-gcd',
    '--pkg=darwin-xpc',
  ]
endif
if host_os_family == 'linux'
  base_vala_args += [
    '--vapidir=' + meson.current_source_dir(),
    '--pkg=frida-linux',
    '--pkg=gio-unix-2.0',
  ]
endif

base_gir_name = f'FridaBase-@api_version@.gir'

base = static_library('frida-base-' + api_version, base_sources,
  c_args: frida_component_cflags,
  vala_args: [base_vala_args, '--pkg=libsoup-3.0'] + extra_vala_args,
  vala_header: 'frida-base.h',
  vala_gir: base_gir_name,
  vala_vapi: f'frida-base-@api_version@.vapi',
  dependencies: [glib_dep, gobject_dep, gio_dep, libsoup_dep, nice_dep, usrsctp_dep, gee_dep, json_glib_dep, gum_dep, openssl_dep] + extra_deps,
  install: have_in_tree_payloads,
  install_dir: [true, header_install_dir, true, false],
)

base_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: base,
  dependencies: [nice_dep, json_glib_dep, gum_dep] + extra_deps,
)

base_build_dir = meson.current_build_dir()

if have_in_tree_payloads
  pkg = import('pkgconfig')
  pkg.generate(base,
    filebase: f'frida-base-@api_version@',
    name: 'Frida Base Library',
    version: frida_version,
    description: 'Basic building blocks',
  )
endif
