fs = import('fs')

generate_api = [python, files('generate.py')]

generate_api_args = [
  meson.current_build_dir(),
  frida_version,
  frida_major_version.to_string(),
  frida_minor_version.to_string(),
  frida_micro_version.to_string(),
  frida_nano_version.to_string(),
  api_version,
  core_build_dir / 'frida-core.h',
  core_build_dir / core_gir_name,
  core_build_dir / 'frida-core.vapi',
  base_build_dir / 'frida-base.h',
  base_build_dir / base_gir_name,
  base_build_dir / f'frida-base-@api_version@.vapi',
]

core_api = custom_target('frida-core-api',
  input: [core, base],
  output: [
    'frida-core.h',
    core_gir_name,
    f'frida-core-@api_version@.vapi',
    f'frida-core-@api_version@.deps',
  ],
  command: [
    generate_api,
    '--output=bundle',
    generate_api_args,
  ],
  install: true,
  install_dir: [header_install_dir, girdir, vapidir, vapidir],
)

core_public_h = core_api[0]
core_public_gir = core_api[1]
core_public_vapi = [core_api[2], core_api[3]]

install_glib_girs = not gi_dep.found() and not fs.exists(girdir / 'GLib-2.0.gir')
glib_gir = fs.copyfile('GLib-2.0.gir', install: install_glib_girs, install_dir: girdir)
gobject_gir = fs.copyfile('GObject-2.0.gir', install: install_glib_girs, install_dir: girdir)
gio_gir = fs.copyfile('Gio-2.0.gir', install: install_glib_girs, install_dir: girdir)

g_ir_compiler = find_program('g-ir-compiler', required: false)
if g_ir_compiler.found()
  custom_target('frida-core-typelib',
    command: [
      g_ir_compiler,
      '--output', '@OUTPUT@',
      '--includedir=' + girdir,
      '--includedir=' + meson.current_source_dir(),
      '@INPUT@',
    ],
    input: core_public_gir,
    output: f'Frida-@api_version@.typelib',
    depends: core,
    install: true,
    install_dir: typelibdir,
  )
endif

core_public_vapi_stamp = custom_target('frida-core-vapi-stamp',
  input: core_public_vapi,
  output: f'frida-core-@api_version@.vapi.stamp',
  command: [
    generate_api,
    '--output=vapi-stamp',
    generate_api_args,
  ],
)

if is_shared
  core_public_lib = core
else
  core_public_lib = custom_target('frida-core-library',
    input: [core] + internal_libs,
    output: f'libfrida-core-@api_version@.a',
    command: [
      python,
      files('merge.py'),
      lib,
      libtool,
      ar,
      meson.current_build_dir(),
      '@OUTPUT@',
      '@INPUT@'
    ],
    install: true,
    install_dir: get_option('libdir'),
  )
endif

core_public_extra_headers = []
if host_os == 'android'
  core_public_extra_headers += fs.copyfile(frida_selinux_h)
endif

if build_machine.system() == 'windows'
  uninstalled_incdir = fs.as_posix(meson.current_build_dir())
else
  uninstalled_incdir = meson.current_build_dir()
endif
uninstalled_girdir = uninstalled_incdir
uninstalled_vapidir = uninstalled_incdir

uninstalled_variables = {
  'frida_includedir': uninstalled_incdir,
  'frida_girdir': uninstalled_girdir,
  'frida_vapidir': uninstalled_vapidir,
}

installed_variables = {
  'frida_includedir': '${includedir}',
  'frida_girdir': girdir,
  'frida_vapidir': vapidir,
}

core_public_dep_kwargs = {
  'sources': [core_public_h, core_public_gir, glib_gir, gobject_gir, gio_gir, core_public_vapi_stamp] + core_public_extra_headers,
  'include_directories': include_directories('.'),
  'link_with': core_public_lib,
  'dependencies': core_deps,
  'variables': uninstalled_variables,
}
if not is_shared
  core_public_dep_kwargs += {
    'link_args': backend_libs_private,
  }
endif
core_public_dep = declare_dependency(kwargs: core_public_dep_kwargs)

core_public_reqs = ['glib-2.0', 'gobject-2.0', 'gio-2.0', 'json-glib-1.0']
if host_os_family == 'windows'
  core_public_reqs += 'gio-windows-2.0'
else
  core_public_reqs += 'gio-unix-2.0'
endif

pkg = import('pkgconfig')
pkg_kwargs = {
  'filebase': f'frida-core-@api_version@',
  'name': 'Frida Core Library',
  'version': frida_version,
  'description': 'Instrumentation toolkit for developers, reverse-engineers, and security researchers',
  'requires': core_public_reqs,
  'subdirs': f'frida-@api_version@',
  'extra_cflags': ['-I${frida_includedir}'],
  'variables': installed_variables,
  'uninstalled_variables': uninstalled_variables,
}
if is_shared
  pkg_kwargs += {
    'libraries': ['-L${libdir}', f'-lfrida-core-@api_version@'],
  }
else
  pkg_kwargs += {
    'libraries': core_public_lib,
    'requires_private': ['gmodule-2.0', 'gee-0.8', 'libsoup-3.0', 'frida-gum-1.0', 'frida-gumjs-inspector-1.0', 'libbrotlidec'] + backend_reqs_private,
    'libraries_private': backend_libs_private,
  }
endif
pkg.generate(kwargs: pkg_kwargs)

meson.override_dependency(f'frida-core-@api_version@', core_public_dep)
