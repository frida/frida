payload_sources = [
  'process.vala',
  'process-glue.c',
  'base-agent-session.vala',
  'script-engine.vala',
  'portal-client.vala',
  'fork-monitor.vala',
  'spawn-monitor.vala',
  'spawn-monitor-glue.c',
  'thread-suspend-monitor.vala',
  'thread-suspend-monitor-glue.c',
  'unwind-sitter.vala',
  'unwind-sitter-glue.c',
  'exit-monitor.vala',
  'cloak.vala',
  'fd-guard.vala',
  'fdt-padder.vala',
  'libc-shim.c',
]

if host_os_family == 'linux'
  payload_sources += 'syscall-linux-@0@.S'.format(host_arch)
endif

extra_c_args = []
extra_vala_args = []
if host_os_family != 'windows'
  extra_vala_args += ['--pkg=posix']
endif
if host_os_family == 'darwin'
  # FIXME: Remove this once glib-2.0.vapi provides a GLib.Private binding not involving deprecated APIs.
  extra_c_args += ['-UGLIB_VERSION_MIN_REQUIRED', '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_30']
  extra_vala_args += ['--pkg=frida-gum-darwin-1.0']
endif
if host_os_family == 'linux'
  extra_vala_args += ['--pkg=linux']
endif

payload_vala_args = gum_vala_args + gumjs_vala_args

payload = static_library('frida-payload-' + api_version, payload_sources,
  c_args: [frida_component_cflags, extra_c_args],
  vala_args: [
    '--vapidir=' + meson.current_source_dir(),
    '--pkg=libc-shim',
    payload_vala_args,
    base_vala_args,
    extra_vala_args,
  ],
  vala_header: 'frida-payload.h',
  vala_vapi: f'frida-payload-@api_version@.vapi',
  dependencies: [gio_dep, gee_dep, json_glib_dep, gum_dep, gumjs_dep, base_dep],
  install: true,
  install_dir: [true, header_install_dir, true],
)

payload_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: payload,
)

pkg = import('pkgconfig')
pkg.generate(payload,
  filebase: f'frida-payload-@api_version@',
  name: 'Frida Payload Library',
  version: frida_version,
  description: 'Building blocks commonly needed to implement payloads',
)
