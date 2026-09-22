symsrv_prefix = get_option('prefix') / get_option('libdir') / 'frida'
uninstalled_symsrv_prefix = meson.current_source_dir()

if host_os_family == 'windows'
  symsrv_dll = files(host_arch / 'symsrv.dll')
  install_data(symsrv_dll, install_dir: symsrv_prefix / host_arch)
endif
