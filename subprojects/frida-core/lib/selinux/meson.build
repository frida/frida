frida_selinux_incdir = include_directories('.')

frida_selinux_h = files('frida-selinux.h')
install_headers(frida_selinux_h, subdir: 'frida-' + api_version)

frida_selinux = static_library('frida-selinux', 'patch.c',
  c_args: frida_component_cflags,
  dependencies: [glib_dep, gio_dep, libselinux_dep, libsepol_dep],
)

frida_selinux_dep = declare_dependency(
  include_directories: frida_selinux_incdir,
  link_with: frida_selinux,
)

frida_selinux_vala_args = ['--pkg=libselinux']
