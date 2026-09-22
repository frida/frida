pipe_sources = [
  'pipe.vala',
]

system_deps = []
extra_vala_args = []

if host_os_family == 'windows'
  pipe_sources += [
    'pipe-windows.c',
    'pipe-sddl.c',
  ]
elif host_os in ['macos', 'ios', 'tvos']
  pipe_sources += 'pipe-darwin.c'
  extra_vala_args += '--pkg=frida-gum-darwin-1.0'
else
  pipe_sources += 'pipe-unix.c'
endif

if host_os_family != 'windows'
  system_deps += gio_unix_dep
  extra_vala_args += ['--pkg=gio-unix-2.0', '--pkg=posix']
endif

if host_os == 'android'
  system_deps += libselinux_dep
  extra_vala_args += '--pkg=libselinux'
endif

pipe = static_library('frida-pipe', pipe_sources,
  c_args: frida_component_cflags,
  vala_args: base_vala_args + extra_vala_args,
  vala_header: 'frida-pipe.h',
  dependencies: [glib_dep, gobject_dep, gio_dep, gee_dep, base_dep] + system_deps,
)

pipe_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: pipe,
)
