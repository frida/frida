dbghelp_prefix = get_option('prefix') / get_option('libdir') / 'frida'
uninstalled_dbghelp_prefix = meson.current_source_dir()

if host_os_family == 'windows'
  dbghelp_dll = files(host_arch / 'dbghelp.dll')
  install_data(dbghelp_dll, install_dir: dbghelp_prefix / host_arch)
endif
