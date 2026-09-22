extra_link_args = []
extra_link_depends = []
if build_os_family == 'darwin'
  symlist = 'executable-no-exports.symbols'
  extra_link_args += ['-Wl,-exported_symbols_list,' + meson.current_source_dir() / symlist]
  extra_link_depends += [symlist]
elif build_os_family != 'windows'
  symscript = (build_os == 'freebsd') ? 'executable-no-exports-freebsd.version' : 'executable-no-exports.version'
  extra_link_args += ['-Wl,--version-script,' + meson.current_source_dir() / symscript]
  extra_link_depends += [symscript]
endif

resource_compiler = executable('frida-resource-compiler', 'resource-compiler.vala',
  vala_args: [
    '--pkg=gio-2.0',
    '--pkg=libbrotlienc',
    '--enable-deprecated',
  ],
  link_args: extra_link_args,
  link_depends: extra_link_depends,
  dependencies: [native_glib_dep, native_gio_dep, native_gee_dep, native_brotlienc_dep],
  native: true,
)

resource_compiler_cmd_array = [
  resource_compiler,
  f'--toolchain=@host_toolchain@',
  f'--machine=@host_arch@'
]
