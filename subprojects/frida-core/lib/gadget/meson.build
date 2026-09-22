gadget_sources = [
  'gadget.vala',
  'gadget-glue.c',
]

if host_os_family == 'darwin'
  gadget_sources += 'gadget-darwin.m'
endif

force_cpp_linking = have_v8
if force_cpp_linking
  gadget_sources += 'dummy.cpp'
endif

platform_deps = []

extra_vala_args = []
extra_objc_args = []
extra_link_args = []
extra_link_depends = []

if host_os_family != 'windows'
  extra_vala_args += '--pkg=gio-unix-2.0'
  platform_deps += gio_unix_dep
endif

if host_os_family == 'darwin'
  extra_objc_args += '-fobjc-arc'
  extra_link_args += '-Wl,-framework,Foundation'
endif

if host_os == 'android'
  extra_vala_args += '--pkg=minizip'
  platform_deps += minizip_dep
endif

if host_os_family == 'windows'
  if host_toolchain != 'microsoft'
    symfile = 'frida-gadget.symbols'
    extra_link_args += '-Wl,--retain-symbols-file,' + meson.current_source_dir() / symfile
    extra_link_depends += symfile
  endif
elif host_os_family == 'darwin'
  symlist = 'frida-gadget.symbols'
  extra_link_args += '-Wl,-exported_symbols_list,' + meson.current_source_dir() / symlist
  extra_link_depends += symlist
else
  symscript = (libc == 'glibc') ? 'frida-gadget-glibc.version' : 'frida-gadget.version'
  extra_link_args += '-Wl,--version-script,' + meson.current_source_dir() / symscript
  extra_link_depends += symscript
endif

if host_os_family in ['linux', 'qnx']
  # For our libc shim to be effective.
  extra_link_args += ['-Wl,-Bsymbolic']
endif

raw_gadget = shared_library('frida-gadget-raw', gadget_sources,
  name_prefix: 'lib',
  vala_args: [base_vala_args, payload_vala_args, extra_vala_args],
  vala_header: 'frida-gadget.h',
  c_args: frida_component_cflags,
  objc_args: extra_objc_args,
  link_args: extra_link_args,
  link_depends: extra_link_depends,
  vs_module_defs: 'frida-gadget.def',
  override_options: ['b_asneeded=true'],
  dependencies: [gio_dep, gee_dep, json_glib_dep, gum_dep, gumjs_dep, base_dep, netif_dep, tls_provider_dep, payload_dep] + platform_deps,
)

operations = [
  '--move', 'constructor', 'frida_libc_shim_init', 'first',
  '--move', 'constructor', 'frida_on_load', 'last',
]
if host_os_family != 'darwin'
  operations += [
    '--move', 'destructor', 'frida_libc_shim_deinit', 'first',
    '--move', 'destructor', 'frida_on_unload', 'last',
  ]
endif
modulated_gadget = custom_target('frida-gadget-modulated',
  input: raw_gadget,
  output: 'libfrida-gadget-modulated' + shlib_suffix,
  command: modulate + operations,
)

need_universal_gadget = host_os_family == 'darwin' and gadget_compat != ''

if host_os == 'macos'
  identity = '@executable_path/../Frameworks/FridaGadget.dylib'
elif host_os in ['ios', 'tvos']
  identity = '@executable_path/Frameworks/FridaGadget.dylib'
else
  identity = gadget_name
endif
gadget_for_host_arch = custom_target('frida-gadget',
  input: modulated_gadget,
  output: need_universal_gadget ? f'@gadget_name@-@host_abi@' : gadget_name,
  command: post_process + ['shared-library', identity],
  install: not need_universal_gadget,
  install_dir: asset_dir,
)

if need_universal_gadget
  gadget = custom_target('frida-gadget-universal',
    input: gadget_for_host_arch,
    output: gadget_name,
    command: [
      lipo,
      '@INPUT@',
      gadget_compat,
      '-create',
      '-output', meson.current_build_dir() / gadget_name,
    ],
    depends: gadget_depends,
    install: true,
    install_dir: asset_dir,
  )
else
  gadget = gadget_for_host_arch
endif
